#include <stdio.h>
#include "klbs.h"

#include "nballoc.c"

#define ITEM_TEST 10

int main(int argc, char**argv){
	unsigned short state;

  	printf("Alloc test:\n");
  	per_cpu_data.nmi = 0;
  	per_cpu_data.cpu_zone = 0;

  	printf("\t| allocating first chunk...");
  	fflush(stdout);
  	void *ptr1 = bd_xx_malloc(4096*1024);
  	printf("done\n");

  	printf("\t| checking state...");
  	fflush(stdout);

  	state = nodes[0].state;
  	assert(nodes[0].prev == NULL);
  	assert(nodes[0].next == NULL); 
	assert(GET_ORDER(state) == MAX_ORDER); 
	assert(GET_REACH(state) == UNLINK); 
	assert(GET_AVAIL(state) == OCC); 
  	printf("done\n");

  	assert(ptr1 == overall_memory);

  	printf("\t| allocating second chunk...");
  	void *ptr2 = bd_xx_malloc(4096);
  	printf("done\n");

  	unsigned int node_idx = 1024;
  	printf("\t| checking state %u...", node_idx);
  	fflush(stdout);

  	state = nodes[node_idx].state;
  	printf("state %x order %u list %u stack %u unlink %u inv %u free %u occ %u ", 
  		state, 
  		GET_ORDER(state), 
  		GET_REACH(state)==LIST, 
  		GET_REACH(state)==STACK, 
  		GET_REACH(state)==UNLINK, 
  		GET_AVAIL(state)==INV, 
  		GET_AVAIL(state)==FREE, 
  		GET_AVAIL(state)==OCC );
	fflush(stdout);
  	
  	assert(nodes[node_idx].prev == NULL);
  	assert(nodes[node_idx].next == NULL); 
	assert(GET_ORDER(state) == 0); 
	assert(GET_REACH(state) == UNLINK); 
	assert(GET_AVAIL(state) == OCC); 
  	printf("done\n");

  	assert(ptr2 == overall_memory+4096*1024);

  	node_idx = 1025;
  	printf("\t| checking state %u...", node_idx);
  	fflush(stdout);
  	state = nodes[node_idx].state;
  	printf("state %x order %u list %u stack %u unlink %u inv %u free %u occ %u ", 
  		state, 
  		GET_ORDER(state), 
  		GET_REACH(state)==LIST, 
  		GET_REACH(state)==STACK, 
  		GET_REACH(state)==UNLINK, 
  		GET_AVAIL(state)==INV, 
  		GET_AVAIL(state)==FREE, 
  		GET_AVAIL(state)==OCC );
	fflush(stdout);
  	
  	assert(nodes[node_idx].prev != NULL);
  	assert(nodes[node_idx].next != NULL); 
	assert(GET_ORDER(state) == 0); 
	assert(GET_REACH(state) == LIST); 
	assert(GET_AVAIL(state) == FREE); 
  	printf("done\n");


  	unsigned int cur_order = 1;
  	while(cur_order < MAX_ORDER){
		node_idx = 1024 + (1<<cur_order);
	  	printf("\t| checking state %u...", node_idx);
	  	fflush(stdout);
	  	state = nodes[node_idx].state;
	  	printf("state %x order %u list %u stack %u unlink %u inv %u free %u occ %u ", 
	  		state, 
	  		GET_ORDER(state), 
	  		GET_REACH(state)==LIST, 
	  		GET_REACH(state)==STACK, 
	  		GET_REACH(state)==UNLINK, 
	  		GET_AVAIL(state)==INV, 
	  		GET_AVAIL(state)==FREE, 
	  		GET_AVAIL(state)==OCC );
		fflush(stdout);
	  	
	  	assert(nodes[node_idx].prev != NULL);
	  	assert(nodes[node_idx].next != NULL); 
		assert(GET_ORDER(state) == cur_order); 
		assert(GET_REACH(state) == LIST); 
		assert(GET_AVAIL(state) == FREE); 
	  	printf("done\n");
	  	cur_order++;
	}


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
  	printf("state %x order %u list %u stack %u unlink %u inv %u free %u occ %u ", 
  		state, 
  		GET_ORDER(state), 
  		GET_REACH(state)==LIST, 
  		GET_REACH(state)==STACK, 
  		GET_REACH(state)==UNLINK, 
  		GET_AVAIL(state)==INV, 
  		GET_AVAIL(state)==FREE, 
  		GET_AVAIL(state)==OCC );
	fflush(stdout);
  	
  	assert(cpu_zones[0].free_pools[MAX_ORDER].free_stack.next == nodes + node_idx);
  	assert(nodes[node_idx].prev == NULL);
	assert(GET_ORDER(state) == cur_order); 
	assert(GET_REACH(state) == STACK); 
	assert(GET_AVAIL(state) == FREE); 
  	printf("done\n");

  	printf("\t| reallocating first chunk from NMI...");
  	fflush(stdout);
  	ptr1 = bd_xx_malloc(4096*1024);
  	printf("done\n");

  	printf("\t| checking state...");
  	fflush(stdout);


  	state = nodes[0].state;
  	
  	printf("state %x order %u list %u stack %u unlink %u inv %u free %u occ %u ", 
  		state, 
  		GET_ORDER(state), 
  		GET_REACH(state)==LIST, 
  		GET_REACH(state)==STACK, 
  		GET_REACH(state)==UNLINK, 
  		GET_AVAIL(state)==INV, 
  		GET_AVAIL(state)==FREE, 
  		GET_AVAIL(state)==OCC );
	fflush(stdout);
  	
  	assert(nodes[0].prev == NULL);
  	assert(nodes[0].next == NULL); 
	assert(GET_ORDER(state) == MAX_ORDER); 
	assert(GET_REACH(state) == UNLINK); 
	assert(GET_AVAIL(state) == OCC); 
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
  	printf("state %x order %u list %u stack %u unlink %u inv %u free %u occ %u ", 
  		state, 
  		GET_ORDER(state), 
  		GET_REACH(state)==LIST, 
  		GET_REACH(state)==STACK, 
  		GET_REACH(state)==UNLINK, 
  		GET_AVAIL(state)==INV, 
  		GET_AVAIL(state)==FREE, 
  		GET_AVAIL(state)==OCC );
	fflush(stdout);
  	
  	assert(cpu_zones[0].free_pools[MAX_ORDER].free_list.head.next == nodes + node_idx);
  	assert(nodes[node_idx].prev == &cpu_zones[0].free_pools[MAX_ORDER].free_list.head);
	assert(nodes[node_idx].next != NULL);
	assert(GET_ORDER(state) == cur_order); 
	assert(GET_REACH(state) == LIST); 
	assert(GET_AVAIL(state) == FREE); 
  	printf("done\n");

}