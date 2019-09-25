#ifndef __1LVL_ALLOC__
#define __1LVL_ALLOC__

typedef struct _node{
    volatile unsigned long long val; //per i bit etc;
    char pad[44];
    unsigned int mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned int mem_size;
    unsigned int pos; //posizione all'interno dell'array "tree"
} node;

#include "nballoc.h"

#endif