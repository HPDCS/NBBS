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

#define SERBATOIO_DIM (16*8192)

#define PAGE_SIZE (4096)

//#define DEBUG
typedef unsigned long long nbint; 

typedef struct _node node;

typedef struct node_container_{
	unsigned long long nodes;
	node* bunch_root;
	char pad[48];
}node_container;

struct _node{
	unsigned long long mem_start; //spiazzamento all'interno dell'overall_memory
	unsigned long long mem_size;
	node_container* container;
	unsigned long long pos; //posizione all'interno dell'array "tree"
#ifdef NUMA
	unsigned long long numa_node; //to remember to which tree the node belongs to
#endif
	unsigned long long container_pos; //posizione all'interno del rispettivo container (1-15)
};



typedef struct _taken_list_elem{
	struct _taken_list_elem* next;
	node* elem;
}taken_list_elem;

typedef struct _taken_list{
	struct _taken_list_elem* head;
	unsigned int number;
}taken_list;


extern unsigned int mypid;
extern unsigned int myid;
extern unsigned int master;
extern unsigned int number_of_leaves;


void init(unsigned long long memory);
void free_node(node* n);
node* request_memory(unsigned int bytes);
void destroy();
extern unsigned int count_occupied_leaves();
extern void write_on_a_file_in_ampiezza();
extern void write_on_a_file_in_ampiezza_start();

#ifdef DEBUG
extern unsigned long long *node_allocated, *size_allocated;
#endif


#endif
