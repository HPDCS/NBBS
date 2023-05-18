#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include "klbs.h"
#include "nballoc.c"

#define ITEM_TEST 10


int main(int argc, char**argv){
	unsigned short state;
	void *tmp;
	unsigned int node_idx  = 0;

  	per_cpu_data.nmi = 0;
  	per_cpu_data.cpu_zone = 0;

	/// ALLOCATING ORDER-10 CHUNK

  	printf("Alloc/Release test (no splitting):\n");
  	assert(nodes[node_idx].address == overall_memory);
  	printf("\t| allocating first chunk...");fflush(stdout);
  	void *ptr1 = bd_xx_malloc(4096*1024);
  	printf("done\n");

  	printf("\t| checking state %u...", node_idx);fflush(stdout);
  	assert_ptr(nodes, NULL, NULL);
  	assert_state(nodes[node_idx].state, MAX_ORDER, OCC, UNLINK);
  	printf("done\n");

  	assert(nodes[node_idx].address == overall_memory);
  	assert(ptr1 == overall_memory);

  	tmp = cpu_zones[0].free_pools[MAX_ORDER].free_list.head.next;

  	printf("\t| releasing first chunk in NOR state...");
  	fflush(stdout);
	bd_xx_free(ptr1);
  	printf("done\n");

  	printf("\t| checking state %u...", node_idx);fflush(stdout);
  	assert_ptr(nodes, &cpu_zones[0].free_pools[MAX_ORDER].free_list.head, tmp);
  	assert_state(nodes[0].state, MAX_ORDER, FREE, LIST);
  	printf("done\n");

  	DISABLE_INTERRUPT();
	printf("\t| allocating first chunk from NMI list...");fflush(stdout);
  	ptr1 = bd_xx_malloc(4096*1024);
  	printf("done\n");
	ENABLE_INTERRUPT();

  	printf("\t| checking state %u...", 0);fflush(stdout);
  	assert_ptr(nodes, &cpu_zones[0].free_pools[MAX_ORDER].free_list.head, tmp);
  	assert_state(nodes[0].state, MAX_ORDER, OCC, LIST);
  	printf("done\n");

	/// ALLOCATING ORDER-0 CHUNK
  	printf("\t| allocating second chunk...");
  	void *ptr2 = bd_xx_malloc(1000);
  	printf("done\n");
 
  	printf("\t| checking state %u...", 0);fflush(stdout);
  	assert_ptr(nodes, NULL, NULL);
  	assert_state(nodes[0].state, MAX_ORDER, OCC, UNLINK);
  	printf("done\n");


  	node_idx = 1024;
  	printf("\t| checking state %u...", node_idx);
  	fflush(stdout);

  	assert_ptr(nodes+node_idx, NULL, NULL);
  	assert_state(nodes[node_idx].state, 0, OCC, UNLINK);
  	fflush(stdout);
  	printf("done\n");

  	assert(ptr2 == overall_memory+4096*1024);

  	node_idx = 1025;
  	printf("\t| checking state %u...", node_idx);
  	fflush(stdout);
  	assert(nodes[node_idx].prev != NULL);
  	assert(nodes[node_idx].next != NULL); 
  	assert_state(nodes[node_idx].state, 0, FREE, LIST);
  	fflush(stdout);
  	printf("done\n");


  	unsigned int cur_order = 1;
  	while(cur_order < MAX_ORDER){
		node_idx = 1024 + (1<<cur_order);
	  	printf("\t| checking state %u...", node_idx);
	  	fflush(stdout);
	  	state = nodes[node_idx].state;
	  	print_state(state);
		fflush(stdout);
	  	
	  	assert(nodes[node_idx].prev != NULL);
	  	assert(nodes[node_idx].next != NULL); 
	  	assert_state(state, cur_order, FREE, LIST);
	  	printf("done\n");
	  	cur_order++;
	}


	/// RELEASING ORDER 10 CHUNK in NMI


  	printf("Free test:\n");
	ENABLE_INTERRUPT();
  	printf("\t| releasing first chunk in NMI state...");
  	fflush(stdout);
	bd_xx_free(ptr1);
  	printf("done\n");

	node_idx = 0;
  	printf("\t| checking state %u...", node_idx);
  	fflush(stdout);
  	state = nodes[node_idx].state;
  	print_state(state);
	fflush(stdout);
  	
  	assert(cpu_zones[0].free_pools[MAX_ORDER].free_stack.next == nodes + node_idx);
  	assert(nodes[node_idx].prev == NULL);
  	assert_state(state, MAX_ORDER, FREE, STACK);
  	printf("done\n");

  	printf("\t| reallocating first chunk from NMI...");
  	fflush(stdout);
  	ptr1 = bd_xx_malloc(4096*1024);
  	printf("done\n");

  	printf("\t| checking state...");
  	fflush(stdout);


  	state = nodes[0].state;
  	
  	print_state(state);
	fflush(stdout);
  	
  	assert(nodes[0].prev == NULL);
  	assert(nodes[0].next == NULL); 
  	assert_state(state, MAX_ORDER, OCC, UNLINK);
  	printf("done\n");

  	assert(ptr1 == overall_memory);

	DISABLE_INTERRUPT();

  	printf("\t| releasing first chunk in NOR state...");
  	fflush(stdout);
	bd_xx_free(ptr1);
  	printf("done\n");

	node_idx = 0;
  	printf("\t| checking state %u...", node_idx);
  	fflush(stdout);
  	state = nodes[node_idx].state;
  	print_state(state);
	fflush(stdout);
  	
  	assert(cpu_zones[0].free_pools[MAX_ORDER].free_list.head.next == nodes + node_idx);
  	assert(nodes[node_idx].prev == &cpu_zones[0].free_pools[MAX_ORDER].free_list.head);
	assert(nodes[node_idx].next != NULL);
  	assert_state(state, MAX_ORDER, FREE, LIST);
  	printf("done\n");





	ENABLE_INTERRUPT();
  	printf("\t| releasing second chunk in NMI state...");
  	fflush(stdout);
	bd_xx_free(ptr2);
  	printf("done\n");

	node_idx = 1024;
  	printf("\t| checking state %u...", node_idx);
  	fflush(stdout);
  	state = nodes[node_idx].state;
  	print_state(state);
	fflush(stdout);
  	
  	assert(cpu_zones[0].free_pools[0].free_stack.next == nodes + node_idx);
  	assert(nodes[node_idx].prev == NULL);
  	assert_state(state, 0, FREE, STACK);
  	printf("done\n");

  	printf("\t| reallocating second chunk from NMI...");
  	fflush(stdout);
  	ptr2 = bd_xx_malloc(4096);
  	printf("done\n");

  	printf("\t| checking state...");	fflush(stdout);
  	assert_ptr(nodes+node_idx, NULL, NULL);
  	assert_state(nodes[node_idx].state, 0, OCC, UNLINK);
  	printf("done\n");

  	assert(ptr2 == overall_memory+4096*1024);

	DISABLE_INTERRUPT();

  	printf("\t| releasing second chunk in NOR state...");fflush(stdout);
	bd_xx_free(ptr2);
  	printf("done\n");

  	printf("\t| checking state %u...", node_idx);	fflush(stdout);
  	assert_state(nodes[node_idx].state, MAX_ORDER, FREE, LIST);
  	printf("done\n");

  	cur_order = 0;
  	while(cur_order < MAX_ORDER){
		node_idx = 1024 + (1<<cur_order);
	  	printf("\t| checking state %u...", node_idx);
	  	fflush(stdout);
	  	state = nodes[node_idx].state;
	  	assert_ptr(nodes+node_idx, NULL, NULL); 
	  	assert_state(state, cur_order, INV, UNLINK);
	  	printf("done\n");
	  	cur_order++;
	}

	destroy();

}