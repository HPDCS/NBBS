#ifndef __4LVL_ALLOC__
#define __4LVL_ALLOC__

#include <pthread.h>

#define BD_SPIN_LOCK 1

#if BD_SPIN_LOCK == 1
	#define BD_LOCK_TYPE pthread_spinlock_t
	#define INIT_BD_LOCK pthread_spinlock_init
	#define BD_LOCK   	 pthread_spin_lock
	#define BD_UNLOCK 	 pthread_spin_unlock
#else
	#define BD_LOCK_TYPE pthread_mutex_t 
	#define INIT_BD_LOCK pthread_mutex_init
	#define BD_LOCK   pthread_mutex_lock
	#define BD_UNLOCK 	 pthread_mutex_unlock
#endif


typedef struct _node node;

typedef struct node_container_{
	unsigned long long nodes;
	node* bunch_root;
	
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



#include "../4lvl-nb/nballoc.h"










#endif