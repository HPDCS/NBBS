#include <stdio.h>
#include "klbs.h"

#include "nballoc.c"

#define ITEM_TEST 10

int main(int argc, char**argv){
  	unsigned int stack_test = ITEM_TEST;
  	unsigned int count = 0;
  	free_stack_t *stack = &cpu_zones[0].free_pools[0].free_stack;

  	printf("Stack test:\n");
  	
	while(stack_test){
		klbd_node_t *node = malloc(sizeof(*node));
		node->state = (unsigned short)stack_test;
		printf("\t| pushing %u...", stack_test);
		fflush(stdout);
		free_stack_push(stack, node);
		printf("done\n");
		stack_test--;
	}

	count = 0;
	klbd_node_t *tmp = stack->next;
	printf("\t| checking size...");
	fflush(stdout);
	while(tmp){
		count++;
		assert(count == tmp->state);
		tmp = tmp->next;
	}

	printf("%d...done\n",count);
	assert(count == ITEM_TEST);
	assert(stack->next->count == ITEM_TEST);

	count = 1;
	while( (tmp=free_stack_pop(stack)) ){
		printf("\t| popping %u...", count);
		fflush(stdout);
		assert(count == tmp->state);
		printf("done\n");
		count++;
		free(tmp);
	}
 
	printf("\t| checking size...");
	fflush(stdout);
	count = 0;
	tmp = stack->next;
	while(tmp){
		count++;
		tmp = tmp->next;
	}
	printf("%d...done\n",count);
	assert(count == 0);

}