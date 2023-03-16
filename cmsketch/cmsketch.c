/*******************************************************************************
***     Author: Tyler Barrus
***     email:  barrust@gmail.com
***     Version: 0.2.0
***     License: MIT 2017
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <inttypes.h>       /* PRIu64 */
#include <math.h>
#include "cmsketch.h"

#define LOG_TWO 0.6931471805599453

/* private functions */
static int __setup_cms(CountMinSketch* cms, uint32_t width, uint32_t depth, double error_rate, double confidence, cms_hash_function hash_function);
static void __write_to_file(CountMinSketch* cms, FILE *fp, short on_disk);
static void __read_from_file(CountMinSketch* cms, FILE *fp, short on_disk, const char* filename);
static void __merge_cms(CountMinSketch* base, int num_sketches, va_list* args);
static int __validate_merge(CountMinSketch* base, int num_sketches, va_list* args);
static uint64_t* __default_hash(unsigned int num_hashes, const char* key);
static uint64_t __fnv_1a(const char* key, int seed);
static int __compare(const void * a, const void * b);
static int32_t __safe_add(int32_t a, uint32_t b);
static int32_t __safe_sub(int32_t a, uint32_t b);
static int32_t __safe_add_2(int32_t a, int32_t b);

// Compatibility with non-clang compilers
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif


int cms_init_optimal_alt(CountMinSketch* cms, double error_rate, double confidence, cms_hash_function hash_function) {
    /* https://cs.stackexchange.com/q/44803 */
    if (error_rate < 0 || confidence < 0) {
        fprintf(stderr, "Unable to initialize the count-min sketch since both error_rate and confidence must be positive!\n");
        return CMS_ERROR;
    }
    uint32_t width = ceil(2 / error_rate);
    uint32_t depth = ceil((-1 * log(1 - confidence)) / LOG_TWO);
    return __setup_cms(cms, width, depth, error_rate, confidence, hash_function);
}

int cms_init_alt(CountMinSketch* cms, uint32_t width, uint32_t depth, cms_hash_function hash_function) {
    if (depth < 1 || width < 1) {
        fprintf(stderr, "Unable to initialize the count-min sketch since either width or depth is 0!\n");
        return CMS_ERROR;
    }
    double confidence = 1 - (1 / pow(2, depth));
    double error_rate = 2 / (double) width;
    return __setup_cms(cms, width, depth, error_rate, confidence, hash_function);
}

int cms_destroy(CountMinSketch* cms) {
    free(cms->bins);
    cms->width = 0;
    cms->depth = 0;
    cms->confidence = 0.0;
    cms->error_rate = 0.0;
    cms->elements_added = 0;
    cms->hash_function = NULL;
    cms->bins = NULL;

    return CMS_SUCCESS;
}

int cms_clear(CountMinSketch* cms) {
    uint32_t i, j = cms->width * cms->depth;
    for (i = 0; i < j; ++i) {
        cms->bins[i] = 0;
    }
    cms->elements_added = 0;
    return CMS_SUCCESS;
}

int32_t cms_add_inc_alt(CountMinSketch* cms, uint64_t* hashes, unsigned int num_hashes, uint32_t x) {
    if (num_hashes < cms->depth) {
        fprintf(stderr, "Insufficient hashes to complete the addition of the element to the count-min sketch!");
        return CMS_ERROR;
    }
    int num_add = INT32_MAX;
    for (unsigned int i = 0; i < cms->depth; ++i) {
        uint64_t bin = (hashes[i] % cms->width) + (i * cms->width);
        cms->bins[bin] = __safe_add(cms->bins[bin], x);
        /* currently a standard min strategy */
        if (cms->bins[bin] < num_add) {
            num_add = cms->bins[bin];
        }
    }
    cms->elements_added += x;
    return num_add;
}

int32_t cms_add_inc(CountMinSketch* cms, const char* key, unsigned int x) {
    uint64_t* hashes = cms_get_hashes(cms, key);
    int32_t num_add = cms_add_inc_alt(cms, hashes, cms->depth, x);
    free(hashes);
    return num_add;
}

int32_t cms_remove_inc_alt(CountMinSketch* cms, uint64_t* hashes, unsigned int num_hashes, unsigned int x) {
    if (num_hashes < cms->depth) {
        fprintf(stderr, "Insufficient hashes to complete the removal of the element to the count-min sketch!");
        return CMS_ERROR;
    }
    int32_t num_add = INT32_MAX;
    for (unsigned int i = 0; i < cms->depth; ++i) {
        uint32_t bin = (hashes[i] % cms->width) + (i * cms->width);
        cms->bins[bin] = __safe_sub(cms->bins[bin], x);
        if (cms->bins[bin] < num_add) {
            num_add = cms->bins[bin];
        }
    }
    cms->elements_added -= x;
    return num_add;
}

int32_t cms_remove_inc(CountMinSketch* cms, const char* key, uint32_t x) {
    uint64_t* hashes = cms_get_hashes(cms, key);
    int32_t num_add = cms_remove_inc_alt(cms, hashes, cms->depth, x);
    free(hashes);
    return num_add;
}

int32_t cms_check_alt(CountMinSketch* cms, uint64_t* hashes, unsigned int num_hashes) {
    if (num_hashes < cms->depth) {
        fprintf(stderr, "Insufficient hashes to complete the min lookup of the element to the count-min sketch!");
        return CMS_ERROR;
    }
    int32_t num_add = INT32_MAX;
    for (unsigned int i = 0; i < cms->depth; ++i) {
        uint32_t bin = (hashes[i] % cms->width) + (i * cms->width);
        if (cms->bins[bin] < num_add) {
            num_add = cms->bins[bin];
        }
    }
    return num_add;
}

int32_t cms_check(CountMinSketch* cms, const char* key) {
    uint64_t* hashes = cms_get_hashes(cms, key);
//    for(int i = 0; i < cms->depth; i++) printf("%"PRIu32" " ,hashes[i]);
    printf("\n");
    int32_t num_add = cms_check_alt(cms, hashes, cms->depth);
    free(hashes);
    return num_add;
}

int32_t cms_check_mean_alt(CountMinSketch* cms, uint64_t* hashes, unsigned int num_hashes) {
    if (num_hashes < cms->depth) {
        fprintf(stderr, "Insufficient hashes to complete the mean lookup of the element to the count-min sketch!");
        return CMS_ERROR;
    }
    int32_t num_add = 0;
    for (unsigned int i = 0; i < cms->depth; ++i) {
        uint32_t bin = (hashes[i] % cms->width) + (i * cms->width);
        num_add += cms->bins[bin];
    }
    return num_add / cms->depth;
}

int32_t cms_check_mean(CountMinSketch* cms, const char* key) {
    uint64_t* hashes = cms_get_hashes(cms, key);
    int32_t num_add = cms_check_mean_alt(cms, hashes, cms->depth);
    free(hashes);
    return num_add;
}

int32_t cms_check_mean_min_alt(CountMinSketch* cms, uint64_t* hashes, unsigned int num_hashes) {
    if (num_hashes < cms->depth) {
        fprintf(stderr, "Insufficient hashes to complete the mean-min lookup of the element to the count-min sketch!");
        return CMS_ERROR;
    }
    int32_t num_add = 0;
    int64_t* mean_min_values = (int64_t*)calloc(cms->depth, sizeof(int64_t));
    for (unsigned int i = 0; i < cms->depth; ++i) {
        uint32_t bin = (hashes[i] % cms->width) + (i * cms->width);
        int32_t val = cms->bins[bin];
        mean_min_values[i] = val - ((cms->elements_added - val) / (cms->width - 1));
    }
    // return the median of the mean_min_value array... need to sort first
    qsort(mean_min_values, cms->depth, sizeof(int64_t), __compare);
    int32_t n = cms->depth;
    if (n % 2 == 0) {
        num_add = (mean_min_values[n/2] + mean_min_values[n/2 - 1]) / 2;
    } else {
        num_add = mean_min_values[n/2];
    }
    free(mean_min_values);
    return num_add;
}

int32_t cms_check_mean_min(CountMinSketch* cms, const char* key) {
    uint64_t* hashes = cms_get_hashes(cms, key);
    int32_t num_add = cms_check_mean_min_alt(cms, hashes, cms->depth);
    free(hashes);
    return num_add;
}

uint64_t* cms_get_hashes_alt(CountMinSketch* cms, unsigned int num_hashes, const char* key) {
    return cms->hash_function(num_hashes, key);
}

int cms_export(CountMinSketch* cms, const char* filepath) {
    FILE *fp;
    fp = fopen(filepath, "w+b");
    if (fp == NULL) {
        fprintf(stderr, "Can't open file %s!\n", filepath);
        return CMS_ERROR;
    }
    __write_to_file(cms, fp, 0);
    fclose(fp);
    return CMS_SUCCESS;
}

int cms_import_alt(CountMinSketch* cms, const char* filepath, cms_hash_function hash_function) {
    FILE *fp;
    fp = fopen(filepath, "r+b");
    if (fp == NULL) {
        fprintf(stderr, "Can't open file %s!\n", filepath);
        return CMS_ERROR;
    }
    __read_from_file(cms, fp, 0, NULL);
    cms->hash_function = (hash_function == NULL) ? __default_hash : hash_function;
    fclose(fp);
    return CMS_SUCCESS;
}

int cms_merge(CountMinSketch* cms, int num_sketches, ...) {
    CountMinSketch* base;
    va_list ap;

    /* Test compatibility */
    va_start(ap, num_sketches);
    int res = __validate_merge(NULL, num_sketches, &ap);
    va_end(ap);

    if (CMS_ERROR == res)
        return CMS_ERROR;

    /* Merge */
    va_start(ap, num_sketches);
    base = (CountMinSketch *) va_arg(ap, CountMinSketch *);
    if (CMS_ERROR == __setup_cms(cms, base->width, base->depth, base->error_rate, base->confidence, base->hash_function)) {
        va_end(ap);
        return CMS_ERROR;
    }
    va_end(ap);

    va_start(ap, num_sketches);
    __merge_cms(cms, num_sketches, &ap);
    va_end(ap);

    return CMS_SUCCESS;
}

int cms_merge_into(CountMinSketch* cms, int num_sketches, ...) {
    va_list ap;

    /* validate all the count-min sketches are of the same dimensions and hash function */
    va_start(ap, num_sketches);
    int res = __validate_merge(cms, num_sketches, &ap);
    va_end(ap);

    if (CMS_ERROR == res)
        return CMS_ERROR;

    /* merge */
    va_start(ap, num_sketches);
    __merge_cms(cms, num_sketches, &ap);
    va_end(ap);

    return CMS_SUCCESS;
}


/*******************************************************************************
*    PRIVATE FUNCTIONS
*******************************************************************************/
static int __setup_cms(CountMinSketch* cms, unsigned int width, unsigned int depth, double error_rate, double confidence, cms_hash_function hash_function) {
    cms->width = width;
    cms->depth = depth;
    cms->confidence = confidence;
    cms->error_rate = error_rate;
    cms->elements_added = 0;
    cms->bins = (int32_t*)calloc((width * depth), sizeof(int32_t));
    cms->hash_function = (hash_function == NULL) ? __default_hash : hash_function;

    if (NULL == cms->bins) {
        fprintf(stderr, "Failed to allocate %zu bytes for bins!", ((width * depth) * sizeof(int32_t)));
        return CMS_ERROR;
    }
    return CMS_SUCCESS;
}

static void __write_to_file(CountMinSketch* cms, FILE *fp, short on_disk) {
    unsigned long long length = cms->depth * cms->width;
    if (on_disk == 0) {
        for (unsigned long long i = 0; i < length; ++i) {
            fwrite(&cms->bins[i], sizeof(int32_t), 1, fp);
        }
    } else {
        // TODO: decide if this should be done directly on disk or not
        // will need to write out everything by hand
        // uint64_t i;
        // int q = 0;
        // for (i = 0; i < length; ++i) {
        //     fwrite(&q, sizeof(int), 1, fp);
        // }
    }
    fwrite(&cms->width, sizeof(int32_t), 1, fp);
    fwrite(&cms->depth, sizeof(int32_t), 1, fp);
    fwrite(&cms->elements_added, sizeof(int64_t), 1, fp);
}

static void __read_from_file(CountMinSketch* cms, FILE *fp, short on_disk, const char* filename) {
    /* read in the values from the file before getting the sketch itself */
    int offset = (sizeof(int32_t) * 2) + sizeof(long);
    fseek(fp, offset * -1, SEEK_END);

    fread(&cms->width, sizeof(int32_t), 1, fp);
    fread(&cms->depth, sizeof(int32_t), 1, fp);
    cms->confidence = 1 - (1 / pow(2, cms->depth));
    cms->error_rate = 2 / (double) cms->width;
    fread(&cms->elements_added, sizeof(int64_t), 1, fp);

    rewind(fp);
    size_t length = cms->width * cms->depth;
    if (on_disk == 0) {
        cms->bins = (int32_t*)malloc(length * sizeof(int32_t));
        size_t read = fread(cms->bins, sizeof(int32_t), length, fp);
        if (read != length) {
            perror("__read_from_file: ");
            exit(1);
        }
    } else {
        // TODO: decide if this should be done directly on disk or not
    }
}

static void __merge_cms(CountMinSketch* base, int num_sketches, va_list* args) {
    int i;
    uint32_t bin, bins = (base->width * base->depth);

    va_list ap;
    va_copy(ap, *args);

    for (i = 0; i < num_sketches; ++i) {
        CountMinSketch *individual_cms = va_arg(ap, CountMinSketch *);
        base->elements_added += individual_cms->elements_added;
        for (bin = 0; bin < bins; ++bin) {
            base->bins[bin] = __safe_add_2(base->bins[bin], individual_cms->bins[bin]);
        }
    }
    va_end(ap);
}


static int __validate_merge(CountMinSketch* base, int num_sketches, va_list* args) {
    int i = 0;
    va_list ap;
    va_copy(ap, *args);

    if (base == NULL) {
        base = (CountMinSketch *) va_arg(ap, CountMinSketch *);
        ++i;
    }

    for (/* skip */; i < num_sketches; ++i) {
        CountMinSketch *individual_cms = va_arg(ap, CountMinSketch *);
        if (!(base->depth == individual_cms->depth
              && base->width == individual_cms->width
              && base->hash_function == individual_cms->hash_function)) {

            fprintf(stderr, "Cannot merge sketches due to incompatible definitions (depth=(%d/%d) width=(%d/%d) hash=(0x%" PRIXPTR "/0x%" PRIXPTR "))",
                    base->depth, individual_cms->depth,
                    base->width, individual_cms->width,
                    (uintptr_t) base->hash_function, (uintptr_t) individual_cms->hash_function);
            va_end(ap);
            return CMS_ERROR;
        }
    }
    return CMS_SUCCESS;
}

/* NOTE: The caller will free the results */
static uint64_t* __default_hash(unsigned int num_hashes, const char* str) {
    uint64_t* results = (uint64_t*)calloc(num_hashes, sizeof(uint64_t));
    int i;
    for (i = 0; i < num_hashes; ++i) {
        results[i] = __fnv_1a(str, i);
    }
    return results;
}

static uint64_t __fnv_1a(const char* key, int seed) {
    // FNV-1a hash (http://www.isthe.com/chongo/tech/comp/fnv/)
    int i, len = strlen(key);
    uint64_t h = 14695981039346656037ULL + (31 * seed); // FNV_OFFSET 64 bit with magic number seed
    for (i = 0; i < len; ++i){
        h = h ^ (unsigned char) key[i];
        h = h * 1099511628211ULL; // FNV_PRIME 64 bit
    }
    return h;
}


static int __compare(const void *a, const void *b) {
    return ( *(int64_t*)a - *(int64_t*)b );
}


static int32_t __safe_add(int32_t a, uint32_t b) {
    if (a == INT32_MAX || a == INT32_MIN) {
        return a;
    }

    /* use the gcc macro if compiling with GCC, otherwise, simple overflow check */
    int32_t c = 0;
#if (__has_builtin(__builtin_add_overflow)) || (defined(__GNUC__) && __GNUC__ >= 5)
    bool bl = __builtin_add_overflow(a, b, &c);
        if (bl) {
            c = INT32_MAX;
        }
#else
    c = ((int64_t) a + b > INT32_MAX) ? INT32_MAX : (a + b);
#endif

    return c;
}

static int32_t __safe_sub(int32_t a, uint32_t b) {
    if (a == INT32_MAX || a == INT32_MIN) {
        return a;
    }

    /* use the gcc macro if compiling with GCC, otherwise, simple overflow check */
    int32_t c = 0;
#if (__has_builtin(__builtin_sub_overflow)) || (defined(__GNUC__) && __GNUC__ >= 5)
    bool bl = __builtin_sub_overflow(a, b, &c);
        if (bl) {
            c = INT32_MIN;
        }
#else
    c = ((int64_t) b - a < INT32_MIN) ? INT32_MAX : (a - b);
#endif

    return c;
}

static int32_t __safe_add_2(int32_t a, int32_t b) {
    if (a == INT32_MAX || a == INT32_MIN) {
        return a;
    }

    /* use the gcc macro if compiling with GCC, otherwise, simple overflow check */
    int64_t c = (int64_t) a + (int64_t) b;
    if (c <= INT32_MIN)
        return INT32_MIN;
    else if (c >= INT32_MAX)
        return INT32_MAX;
    return (int32_t) c;
}