#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/wait.h>

//bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval)

#define OCCUPY ((unsigned long) (0x10))
#define MASK_CLEAN_LEFT_COALESCE ((unsigned long)(~(MASK_LEFT_COALESCE)))
#define MASK_CLEAN_RIGHT_COALESCE ((unsigned long)(~(MASK_RIGHT_COALESCE)))
#define MASK_OCCUPY_RIGHT ((unsigned long) (0x1))
#define MASK_OCCUPY_LEFT ((unsigned long) (0x2))
#define MASK_LEFT_COALESCE ((unsigned long) (0x8))
#define MASK_RIGHT_COALESCE ((unsigned long) (0x4))
//PARAMETRIZZAZIONE
#ifndef MIN_ALLOCABLE_BYTES
#define MIN_ALLOCABLE_BYTES 8 //(2KB) numero minimo di byte allocabili
#endif
#ifndef MAX_ALLOCABLE_BYTE
#define MAX_ALLOCABLE_BYTE  16384 //(16KB)
#endif
#define FREE_BLOCK ((unsigned long) 0)
#define OCCUPY_BLOCK ((OCCUPY) | (MASK_OCCUPY_LEFT) | (MASK_OCCUPY_RIGHT))
#define MASK_CLEAN_OCCUPIED_LEFT (~(MASK_OCCUPY_LEFT))
#define MASK_CLEAN_OCCUPIED_RIGHT (~(MASK_OCCUPY_RIGHT))

#define ROOT (tree[1])
#define left(n) (tree[((n->pos)*(2))])
#define right(n) (tree[(((n->pos)*(2))+(1))])
#define left_index(n) (((n)->pos)*(2))
#define right_index(n) ((((n)->pos)*(2))+(1))
#define parent(n) (tree[(unsigned)(((n)->pos)/(2))])
#define parent_index(n) ((unsigned)(((n)->pos)/(2)))
#define level(n) ((unsigned) ( (overall_height) - (log2_(( (n)->mem_size) / (MIN_ALLOCABLE_BYTES )) )))
#define SERBATOIO_DIM (16*8192)

#define PAGE_SIZE (4096)

typedef struct _node{
    unsigned long mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned long mem_size;
    unsigned long  val; //per i bit etc;
    unsigned pos; //posizione all'interno dell'array "tree"
} node;

extern unsigned mypid;

void init(unsigned long memory);
void free_nodes(node* n); //questo fa la free fisicamente
void free_node(node* n);
node* request_memory(unsigned pages);
unsigned log2_(unsigned long value);
unsigned rand_lim(unsigned limit);
