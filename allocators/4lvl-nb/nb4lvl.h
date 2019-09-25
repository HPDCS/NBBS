#ifndef __4LVL_ALLOC__
#define __4LVL_ALLOC__

typedef struct _node node;

typedef struct node_container_{
	unsigned long long nodes;
	node* bunch_root;
	char pad[52];
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

#include "nballoc.h"

#endif