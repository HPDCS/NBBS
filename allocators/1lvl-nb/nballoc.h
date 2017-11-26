#ifndef __NB_ALLOC__
#define __NB_ALLOC__
/****************************************************
				ALLOCATOR PARAMETES
****************************************************/

#ifndef MIN_ALLOCABLE_BYTES
#define MIN_ALLOCABLE_BYTES 8ULL //(2KB) numero minimo di byte allocabili
#endif
#ifndef MAX_ALLOCABLE_BYTE
#define MAX_ALLOCABLE_BYTE  16384ULL //(16KB)
#endif
#ifndef NUM_LEVELS
#define NUM_LEVELS  20ULL //(16KB)
#endif


#define PAGE_SIZE (4096)

//#define DEBUG

typedef unsigned long long nbint; 


typedef struct _node{
    volatile nbint val; //per i bit etc;
    char pad[48];
    unsigned int mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned int mem_size;
    unsigned int pos; //posizione all'interno dell'array "tree"
} node;


typedef struct _taken_list_elem{
    struct _taken_list_elem* next;
    node* elem;
}taken_list_elem;

typedef struct _taken_list{
    struct _taken_list_elem* head;
    unsigned number;
}taken_list;


extern void write_on_a_file_in_ampiezza_start();
extern __thread unsigned myid;
extern unsigned int number_of_leaves;

void  bd_xx_free(void* n);
void* bd_xx_malloc(size_t pages);

#ifdef DEBUG
extern unsigned long long *node_allocated; 
extern nbint *size_allocated;
#endif


#endif
