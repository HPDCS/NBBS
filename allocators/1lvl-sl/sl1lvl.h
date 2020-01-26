#ifndef __1LVL_ALLOC__
#define __1LVL_ALLOC__

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

typedef struct _node{
    unsigned long long val; //per i bit etc;
    unsigned int mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned int mem_size;
    unsigned int pos; //posizione all'interno dell'array "tree"
} node;

#include "../1lvl-nb/nballoc.h"


#endif