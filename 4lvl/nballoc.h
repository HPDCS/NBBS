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

typedef struct _node node;

typedef struct node_container_{
    unsigned long nodes;
    node* bunch_root;
}node_container;

struct _node{
    unsigned long mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned long mem_size;
    node_container* container;
    unsigned pos; //posizione all'interno dell'array "tree"
#ifdef NUMA
    unsigned numa_node; //to remember to which tree the node belongs to
#endif
    char container_pos; //posizione all'interno del rispettivo container (1-15)
};



typedef struct _taken_list_elem{
    struct _taken_list_elem* next;
    node* elem;
}taken_list_elem;

typedef struct _taken_list{
    struct _taken_list_elem* head;
    unsigned number;
}taken_list;


extern unsigned mypid;
extern unsigned master;


void init(unsigned long memory);
void free_node(node* n);
node* request_memory(unsigned pages);
void destroy();



#endif
