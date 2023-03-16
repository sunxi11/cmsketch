#include <stdio.h>
#include "cmsketch/cmsketch.h"
#include "string.h"
#include "stdlib.h"
CountMinSketch cms;


uint32_t quary(CountMinSketch *cms, const char* key){
    uint64_t *hashs = cms->hash_function(cms->depth, key);
    uint64_t hash = 0;
    int32_t num = INT32_MAX;
    for(int i = 0; i < cms->depth; i++) printf("%ld"" " ,hashs[i]);
    printf("\n");
    for(int i = 0; i < cms->depth; i++){
//        hash = hashs[i];

        hash = cms->bins[hashs[i] % cms->width + i * cms->width];
        if(hash < num) num = hash;
    }
    free(hashs);
    return num;
}

void add(CountMinSketch* cms, const char* key){
    uint64_t* hashs = cms->hash_function(cms->depth, key);
    uint64_t bin;
    for(int i = 0; i < cms->depth; i++){
        bin = hashs[i] % cms->width + i * cms->width;
        cms->bins[bin] += 1;
    }
    free(hashs);
}

int main(){
    cms_init(&cms, 10000, 7);

    int i, res;
    uint32_t r;
    for (i = 0; i < 10; i++) {
//        res = add(&cms, "this is a test");
        add(&cms, "this is a test");
    }

    r = quary(&cms, "this is a test");
    printf("%d", r);
    if (r != 10) {
        printf("Error with lookup: %d\n", r);
    }
    cms_destroy(&cms);
    return 0;
};

/* NOTE: The caller will free the results */
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


static uint64_t* __default_hash(unsigned int num_hashes, const char* str) {
//    uint64_t* results = (uint64_t*)calloc(num_hashes, sizeof(uint64_t));
//    int i;
//    for (i = 0; i < num_hashes; ++i) {
//        results[i] = __fnv_1a(str, i);
//    }
//    return results;

    uint64_t* results = (uint64_t*)calloc(num_hashes, sizeof(uint64_t));
    for(int i= 0; i < num_hashes; i++)
        results[i] = __fnv_1a(str, i);
    return results;
}


