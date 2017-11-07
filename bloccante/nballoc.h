#ifndef __NB_ALLOC__
#define __NB_ALLOC__

/****************************************************
				ALLOCATOR PARAMETES
****************************************************/

#ifndef MIN_ALLOCABLE_BYTES
#define MIN_ALLOCABLE_BYTES 8 //(2KB) numero minimo di byte allocabili
#endif
#ifndef MAX_ALLOCABLE_BYTE
#define MAX_ALLOCABLE_BYTE  16384 //(16KB)
#endif

#define SERBATOIO_DIM (16*8192)

#define PAGE_SIZE (4096)

typedef struct _node{
    unsigned int mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned int mem_size;
    volatile unsigned int val; //per i bit etc;
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


extern unsigned mypid;
extern unsigned myid;

void init(unsigned long memory);
void free_node(node* n);
node* request_memory(unsigned pages);
void destroy();

void print_in_ampiezza();



#endif
