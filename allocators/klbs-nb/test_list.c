#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include "klbs.h"

#include "nballoc.c"

#define ITEM_TEST 10

int main(int argc, char**argv){
  	klbd_node_t nodes[ITEM_TEST];
  	unsigned int list_test = ITEM_TEST;
  	unsigned int count = 0;
  	free_list_t *list = &cpu_zones[0].free_pools[0].free_list;
  	per_cpu_data.cpu_zone = 0;
  	per_cpu_data.nmi = 0;
  	printf("List test:\n");
  	
  	count = 0;
	klbd_node_t *tmp = list->head.next;
	printf("\t| checking size...");
	fflush(stdout);
	while(tmp != &list->tail){
		count++;
		tmp = tmp->next;
	}
	printf("%d...done\n",count);
	assert(count == 0);

  	count = 0;
	while(list_test){
		klbd_node_t *node = nodes+count;
		node->next=NULL;
		node->prev=NULL;
		node->zone=0;
		node->state = (unsigned short)list_test;
		printf("\t| insert %u@%p...", list_test,nodes+count);
		fflush(stdout);
		assert(free_list_insert(list, node));
		printf("done\n");
		list_test--;
		count++;
	}

	count = 0;
	tmp = list->head.next;
	printf("\t| checking size...");
	fflush(stdout);
	while(tmp != &list->tail){
		count++;
		tmp = tmp->next;
	}

	printf("%d...done\n",count);
	assert(count == ITEM_TEST);

	count = 0;
	while(count<ITEM_TEST){
		assert(free_list_remove(nodes+count));
		printf("\t| popping %u...", count+1);
		fflush(stdout);
		printf("done\n");
		count++;
	}
 
	printf("\t| checking size...");
	fflush(stdout);
	count = 0;
	tmp = list->head.next;
	while(tmp != &list->tail){
		count++;
		tmp = tmp->next;
	}
	printf("%d...done\n",count);
	assert(count == 0);

}