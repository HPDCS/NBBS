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

#define BD_SPIN_LOCK 1

#if BD_SPIN_LOCK == 1
	#define BD_LOCK_TYPE pthread_spinlock_t 
	#define INIT_BD_LOCK pthread_spinlock_init
	#define BD_LOCK   	 pthread_spin_lock
	#define BD_UNLOCK 	 pthread_spin_unlock
#else
	#define BD_LOCK_TYPE pthread_mutex_t 
	#define INIT_BD_LOCK pthread_mutex_init
	#define BD_LOCK   	 pthread_mutex_lock
	#define BD_UNLOCK 	 pthread_mutex_unlock
#endif

typedef unsigned long long nbint; 

typedef struct _node node;

typedef struct node_container_{
	unsigned long long nodes;
	node* bunch_root;
	
}node_container;

struct _node{
	BD_LOCK_TYPE lock;
	char pad[48];
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


extern __thread unsigned int myid;
extern unsigned int number_of_leaves;

void* allocate(unsigned int order);
void deallocate(void* address, unsigned int order);


#ifdef DEBUG
extern unsigned long long *node_allocated, *size_allocated;
#endif


#endif
