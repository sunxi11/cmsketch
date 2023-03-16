//
// Created by DELL on 2023/3/16.
//
#include <stdio.h>
#include <stdlib.h>


int main()
{
    int n = 7;
    int **p = (int **)malloc(7*sizeof(int*));
    for(int i=0; i < n; i++){
        p[i] = (int *) malloc(10*sizeof(int));
    }
    for(int i = 0; i < 7; i++){
        for(int j = 0; j < 10; j++){
            p[i][j] = i * 10 + j;
        }
    }
    printf("%d", p[2][3]);
    for(int i = 0; i < n; i++){
        free(p[i]);
    }
    free(p);

    return 0;
}