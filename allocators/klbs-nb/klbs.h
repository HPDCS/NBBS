#ifndef __KLBS_H__
#define __KLBS_H__

#include <stdbool.h>

#include "nballoc.h"

#define MAX_ORDER 						(11U)
#define NUM_ORDERS						(MAX_ORDER+1)
#define NUMBER_OF_LEAVES            	((1ULL) << (NUM_LEVELS  -1))
#define NUMBER_OF_CPUS					(20U)
#define MEMORY_SIZE 					(MIN_ALLOCABLE_BYTES * NUMBER_OF_LEAVES)
#define NUMBER_OF_MAX_NODES				(MEMORY_SIZE/MAX_ALLOCABLE_BYTES)
#define NUMBER_OF_MAX_NODES_PER_CPU  	(NUMBER_OF_MAX_NODES/NUMBER_OF_CPUS)
#define NUMBER_OF_LEAVES_PER_CPU_ZONE  	(NUMBER_OF_MAX_NODES_PER_CPU << MAX_ORDER)
#define MEMORY_PER_CPU_ZONE				(NUMBER_OF_LEAVES_PER_CPU_ZONE*MIN_ALLOCABLE_BYTES)

typedef struct __per_cpu_data{
	int cpu_zone;
	volatile unsigned int nmi; 
} cpu_data_t;

typedef struct __klbs_node_t{
	struct __klbs_node_t *prev;
	struct __klbs_node_t *next;
	char *address;
	unsigned int idx;
	unsigned int count;
	unsigned int zone;
	unsigned short state;
} klbd_node_t;


typedef struct __free_list{
	struct __klbs_node_t head;
	struct __klbs_node_t tail;	
} free_list_t;

typedef struct __free_stack{
	struct __klbs_node_t * volatile next;
} free_stack_t;


typedef struct __free_pool{
	free_list_t  free_list;
	free_stack_t free_stack;
} free_pool_t;


typedef struct __cpu_zone{
	free_pool_t free_pools[NUM_ORDERS];
} cpu_zone_t;



#endif