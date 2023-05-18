#if !defined(KERNEL_BD) || KERNEL_BD==0  
#define _GNU_SOURCE             
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>

#include "utils.h"
#include "klbs.h"


#define FREE    	(0x0)
#define INV     	(0x100)
#define OCC     	(0x200)

#define LIST    	(0x400)
#define STACK   	(0x800)
#define UNLINK 		(0x0)

#define AVAIL_MASK 	(INV|OCC)
#define REACH_MASK 	(LIST|STACK)
#define ORDER_MASK	(0xff)

#define GET_REACH(x)	((x)&REACH_MASK)
#define GET_AVAIL(x)	((x)&AVAIL_MASK)
#define GET_ORDER(x)	((x)&ORDER_MASK)


#define MIN(x,y)  (  (x)<(y) ? (x) : (y)  )

#define GET_ZONE_BY_IDX(x) 				(MIN((NUMBER_OF_CPUS-1),(x)/NUMBER_OF_LEAVES_PER_CPU_ZONE))
#define GET_ZONE_BY_PTR(x) 				((x)->zone)
#define GET_CURRENT_CPU_ZONE() 			(per_cpu_data.cpu_zone)
#define GET_ADDRESS_FROM_NODE(x)		((x)->address)
#define IS_NMI() 						(per_cpu_data.nmi >= 1)


static unsigned int partecipants=0;
static volatile int init_phase                  = 0;

static void* volatile overall_memory            = NULL;
static unsigned long long overall_memory_size   = MEMORY_SIZE;

static klbd_node_t nodes[NUMBER_OF_LEAVES];
static cpu_zone_t cpu_zones[NUMBER_OF_CPUS];


#if !defined(KERNEL_BD) || KERNEL_BD==0  
#include <sched.h>
#include <pthread.h>
__thread cpu_data_t per_cpu_data = {
	.cpu_zone = -1,
	.nmi = 0
};
#define DISABLE_MIGRATION() 			do{\
if(per_cpu_data.cpu_zone == -1){\
per_cpu_data.nmi = 0;\
per_cpu_data.cpu_zone = __sync_fetch_and_add(&partecipants, 1) % NUMBER_OF_CPUS;\
cpu_set_t set;\
CPU_ZERO(&set);\
CPU_SET(per_cpu_data.cpu_zone, &set);\
pthread_setaffinity_np(pthread_self(), sizeof(set),&set);\
printf("mycpu zone %u\n", per_cpu_data.cpu_zone);\
}\
}while(0)

#define ENABLE_MIGRATION() 				do{}while(0)

#define DISABLE_INTERRUPT() 			do{\
__sync_fetch_and_add(&per_cpu_data.nmi, 1);\
}while(0)

#define ENABLE_INTERRUPT() 				do{\
__sync_fetch_and_add(&per_cpu_data.nmi, -1);\
}while(0)

#else

#endif





static inline bool is_left_buddy_from_idx(unsigned int x, unsigned short o){
	return ((x >> o) & 1) == 0;
} 

static inline klbd_node_t* get_buddy(klbd_node_t* node){
	return nodes + (node->idx ^ (1<<GET_ORDER(node->state)));
}

static inline klbd_node_t* get_buddy_at_order(klbd_node_t* node, unsigned short order){
	return nodes + (node->idx ^ (1<<order));
}


bool free_list_insert(free_list_t *list, klbd_node_t *node);

void print_state(unsigned short state){
  	printf("state %x order %u list %u stack %u unlink %u inv %u free %u occ %u ", 
  		state, 
  		GET_ORDER(state), 
  		GET_REACH(state)==LIST, 
  		GET_REACH(state)==STACK, 
  		GET_REACH(state)==UNLINK, 
  		GET_AVAIL(state)==INV, 
  		GET_AVAIL(state)==FREE, 
  		GET_AVAIL(state)==OCC );	
}

void assert_state(unsigned short state, unsigned short order, unsigned short avail, unsigned short reach){
	print_state(state);fflush(stdout);
	assert(GET_ORDER(state) == order);
	assert(GET_AVAIL(state) == avail);
	assert(GET_REACH(state) == reach);
}

void assert_ptr(klbd_node_t *node, void *p, void *n){
	assert(node->prev == p);
	assert(node->next == n);
}

/*
 This function inits a static tree represented as an implicit binary heap. 
 The first node at index 0 is a dummy node.
 */
static void init_tree(){
	printf("Init tree\n");
    unsigned long i,j;
    short cur_order;
    unsigned long long offset;
    unsigned int  cur_zone;
  	unsigned int count_max, expected, total;
    
  	printf("NUMBER_OF_MAX_NODES %llu\n", NUMBER_OF_MAX_NODES);
  	printf("NUMBER_OF_MAX_NODES_PER_CPU %llu\n", NUMBER_OF_MAX_NODES_PER_CPU);
  	printf("NUMBER_OF_LEAVES %llu\n", NUMBER_OF_LEAVES);
  	printf("NUMBER_OF_LEAVES_PER_CPU_ZONE %llu\n", NUMBER_OF_LEAVES_PER_CPU_ZONE);

    printf("\t| Init zones...");
	fflush(stdout);
    for(i=0; i<NUMBER_OF_CPUS;i++){
    	for(j=0;j<NUM_ORDERS;j++){
	    	cpu_zones[i].free_pools[j].free_stack.next = NULL;

	    	cpu_zones[i].free_pools[j].free_list.tail.zone = i;
			cpu_zones[i].free_pools[j].free_list.head.zone = i;
			
			cpu_zones[i].free_pools[j].free_list.tail.address = (void*) 0xffff;
			cpu_zones[i].free_pools[j].free_list.head.address = (void*) 0xaaaa;;

			cpu_zones[i].free_pools[j].free_list.head.next = &cpu_zones[i].free_pools[j].free_list.tail;
			cpu_zones[i].free_pools[j].free_list.head.prev = NULL;
			cpu_zones[i].free_pools[j].free_list.tail.prev = &cpu_zones[i].free_pools[j].free_list.head;
			cpu_zones[i].free_pools[j].free_list.tail.next = NULL;
    	}
    }
	printf("Done\n");

	for(i=0;i<NUMBER_OF_CPUS;i++){
		for(unsigned int j=0;j<NUM_ORDERS;j++){
			assert(cpu_zones[i].free_pools[j].free_stack.next == NULL);
			assert(cpu_zones[i].free_pools[j].free_list.head.prev == NULL);
			assert(cpu_zones[i].free_pools[j].free_list.tail.next == NULL);
			assert(cpu_zones[i].free_pools[j].free_list.head.next == &cpu_zones[i].free_pools[j].free_list.tail);
			assert(cpu_zones[i].free_pools[j].free_list.tail.prev == &cpu_zones[i].free_pools[j].free_list.head);
		}
	}

	printf("\t| First pass on nodes...");
	fflush(stdout);
	unsigned int check = 0;
	unsigned int check2 = 0;
	int last_zone = -1;
    for(i=0;i<NUMBER_OF_LEAVES;i++){
    	nodes[i].idx = i;
    	nodes[i].next = nodes[i].prev = NULL;
    	nodes[i].address = overall_memory + i*MIN_ALLOCABLE_BYTES;
		nodes[i].zone  = GET_ZONE_BY_IDX(i);
		if(last_zone != nodes[i].zone){
		//	printf("CHECCK %u %u\n", i, NUMBER_OF_LEAVES_PER_CPU_ZONE, i/NUMBER_OF_LEAVES_PER_CPU_ZONE);
		//	printf("check first of zone 1 %u\n", i);
			last_zone++;
		}
    	nodes[i].count = 0;
    	nodes[i].state = 0;
	}
	printf("Done\n");

	for(i=0;i<NUMBER_OF_CPUS;i++){
		for(unsigned int j=0;j<NUM_ORDERS;j++){
			assert(cpu_zones[i].free_pools[j].free_stack.next == NULL);
			assert(cpu_zones[i].free_pools[j].free_list.head.prev == NULL);
			assert(cpu_zones[i].free_pools[j].free_list.tail.next == NULL);
			assert(cpu_zones[i].free_pools[j].free_list.head.next == &cpu_zones[i].free_pools[j].free_list.tail);
			assert(cpu_zones[i].free_pools[j].free_list.tail.prev == &cpu_zones[i].free_pools[j].free_list.head);
		}
	}

    cur_order = MAX_ORDER;
	offset = 1 << cur_order;
	count_max = 0;
	total = 0;
	expected = NUMBER_OF_MAX_NODES;
	printf("\t| Second pass on nodes...%llu...", offset);
	fflush(stdout);
	printf("\n");
	check = 0;
    for(i=0;i<NUMBER_OF_LEAVES;i+=offset){
		if(!check && nodes[i].zone){
//			printf("check first of zone 1 %u\n", i);
			check=1;
		}
    	count_max++;
    	nodes[i].state = MAX_ORDER | LIST | FREE;
    	cpu_zone_t *zone = cpu_zones + nodes[i].zone;
    	free_pool_t *pool= zone->free_pools + cur_order;
    	//free_list_insert(&pool->free_list, nodes+i);

		nodes[i].next = &pool->free_list.tail;
		nodes[i].prev = pool->free_list.tail.prev;
		nodes[i].next->prev = nodes+i;
		nodes[i].prev->next = nodes+i; 
		//printf("ADDING zone %u %u\n", nodes[i].zone, i);
	}
	printf("Done\n");

	for(i=0;i<NUMBER_OF_CPUS;i++){

		for(unsigned int j=0;j<NUM_ORDERS;j++){
			assert(cpu_zones[i].free_pools[j].free_stack.next == NULL);
			assert(cpu_zones[i].free_pools[j].free_list.head.prev == NULL);
			assert(cpu_zones[i].free_pools[j].free_list.tail.next == NULL);

			if(j!=MAX_ORDER){
				assert(cpu_zones[i].free_pools[j].free_list.head.next == &cpu_zones[i].free_pools[j].free_list.tail);
				assert(cpu_zones[i].free_pools[j].free_list.tail.prev == &cpu_zones[i].free_pools[j].free_list.head);
			}
			else{
				//printf("BEG %u %u\n", i,j);
				klbd_node_t *tmp = &cpu_zones[i].free_pools[j].free_list.head;
				while(tmp != &cpu_zones[i].free_pools[j].free_list.tail){
				//	printf("%p\n", tmp);
					tmp = tmp->next;
				}
				//printf("END %u %u\n", i,j);
			}
		}
	}
	assert(count_max == (expected-total));

	printf("\t| Third pass on nodes...");
	fflush(stdout);
	while(--cur_order > 0){
		offset = 1 << cur_order;
		expected *= 2;
		total += count_max;
		count_max = 0;
	    for(i=0;i<NUMBER_OF_LEAVES;i+=offset){
	       	if(nodes[i].state) continue;
	       	count_max++;
    		nodes[i].state = cur_order | UNLINK | INV;
	    	nodes[i].next = nodes[i].prev = NULL;
	    	nodes[i].address = overall_memory + i*MIN_ALLOCABLE_BYTES;
			nodes[i].zone  = GET_ZONE_BY_IDX(i);
	    	nodes[i].count = 0;
		}
		assert(count_max == (expected-total));
	} 
	printf("Done\n");
}

/*
 This function destroy the Non-Blocking Buddy System.
 */
void destroy(){
    munmap(overall_memory, overall_memory_size);
}


/*
 This function build the Non-Blocking Buddy System.
 */
void init(){
    void *tmp_overall_memory;
    bool first = false;
    
    if(overall_memory_size < MAX_ALLOCABLE_BYTES) NB_ABORT("No enough levels\n");

    
    if(init_phase ==  0 && __sync_bool_compare_and_swap(&init_phase, 0, 1)){

        tmp_overall_memory = mmap(NULL, overall_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        if(tmp_overall_memory == MAP_FAILED) 
            NB_ABORT("No enough levels\n");
        else if(__sync_bool_compare_and_swap(&overall_memory, NULL, tmp_overall_memory)){
        	first = true;
        	init_tree();
        }
        else
            munmap(tmp_overall_memory, overall_memory_size);
            
#ifdef DEBUG
    printf("Debug mode: ON\n");
#endif

        __sync_bool_compare_and_swap(&init_phase, 1, 2);
    }

    while(init_phase < 2);

    if(first){
        printf("klbd-nb: UMA Init complete\n");
        printf("\t Total Memory = %lluB, %.0fKB, %.0fMB, %.0fGB\n" , overall_memory_size, overall_memory_size/1024.0, overall_memory_size/1048576.0, overall_memory_size/1073741824.0);
        printf("\t Nodes  = %10llu\n", NUMBER_OF_LEAVES);
        printf("\t Min size %12lluB, %.0fKB, %.0fMB, %.0fGB\n"   , MIN_ALLOCABLE_BYTES, MIN_ALLOCABLE_BYTES/1024.0, MIN_ALLOCABLE_BYTES/1048576.0, MIN_ALLOCABLE_BYTES/1073741824.0);//, overall_height);
        printf("\t Max size %12lluB, %.0fKB, %.0fMB, %.0fGB\n"   , MAX_ALLOCABLE_BYTES, MAX_ALLOCABLE_BYTES/1024.0, MAX_ALLOCABLE_BYTES/1048576.0, MAX_ALLOCABLE_BYTES/1073741824.0);//, overall_height - log2_(MAX_ALLOCABLE_BYTES/MIN_ALLOCABLE_BYTES));
        //printf("\t Max allocable level %2llu\n", max_level);
    }


    if(init_phase == 2) 
        return;

}





void __attribute__ ((constructor(500))) premain(){ init(); }


void free_stack_push(free_stack_t *stack, klbd_node_t *node){
	klbd_node_t *old_head;
	do{
		old_head = stack->next;
		node->next = old_head;
		node->count= 1;
		if(old_head) node->count += old_head->count;
	}while(!__sync_bool_compare_and_swap(&stack->next, old_head, node));
}

klbd_node_t* free_stack_pop(free_stack_t *stack){
	klbd_node_t *old_head = NULL;
	do{
		old_head = stack->next;
	}while(old_head &&!__sync_bool_compare_and_swap(&stack->next, old_head, old_head->next));
	return old_head;
}


bool free_list_insert(free_list_t *list, klbd_node_t *node){
	klbd_node_t *head = &list->head;
	bool res = false;
	if(GET_CURRENT_CPU_ZONE() != GET_ZONE_BY_PTR(node) || IS_NMI())
		return res;

	DISABLE_INTERRUPT();
	if(!node->prev){
		res = true;
		node->prev = head;
		node->next = head->next;
		node->next->prev = node;
		node->prev->next = node; 
	}	
	ENABLE_INTERRUPT();
	return res;

}

bool free_list_remove(klbd_node_t *node){
	bool res = false;
	if(GET_CURRENT_CPU_ZONE() != GET_ZONE_BY_PTR(node) || IS_NMI())
		return res;

	DISABLE_INTERRUPT();
	if(node->prev){
		res = true;
		node->next->prev = node->prev;
		node->prev->next = node->next; 
		node->prev = NULL;
		node->next = NULL;
	}	
	ENABLE_INTERRUPT();
	return res;

}

klbd_node_t* get_free_node(cpu_zone_t *z, unsigned short o){
	free_list_t *list;
	unsigned short cur_state, new_state;
	klbd_node_t *node, *next;

begin:
	list = &z->free_pools[o].free_list;
	node = list->head.next;
		
	while(true){
		if(node == &list->tail){
			node = NULL;
		  #ifdef TEST
			printf("EMPTY ORDER %u\n", o);
		  #endif
		 	break;
		}
		cur_state = node->state;
		next = node->next;
//		printf("FOUND %p next %p head %p tail %p ", node, next, &list->head, &list->tail);
//		print_state(cur_state);fflush(stdout); 
	  #ifdef TEST
		printf("FOUND %p ", node);
		print_state(cur_state);fflush(stdout); 
	  #endif
		if(GET_REACH(cur_state) != LIST) goto begin;
		if(next == NULL)  goto begin;
		if(GET_AVAIL(cur_state) == FREE){
			if(GET_ORDER(cur_state) >= o){
				new_state = GET_ORDER(cur_state) | GET_REACH(cur_state) | OCC;
				if(__sync_bool_compare_and_swap(&node->state, cur_state, new_state)){
					if(free_list_remove(node)){
						cur_state = new_state;
						new_state = GET_ORDER(cur_state) | GET_AVAIL(cur_state) | UNLINK;
						__sync_bool_compare_and_swap(&node->state, cur_state, new_state);
					}
					return node;
				}
			}
		}
		if(GET_AVAIL(cur_state) == OCC){
			if(free_list_remove(node)){
				new_state = GET_ORDER(cur_state) | GET_AVAIL(cur_state) | UNLINK;
				__sync_bool_compare_and_swap(&node->state, cur_state, new_state);
			}

		}
		node = next;
	}
	return node;
}


void split(klbd_node_t *node){
	klbd_node_t *buddy;
	unsigned short cur_state, new_state, order;
	__sync_fetch_and_add(&node->state, -1);
	order = GET_ORDER(node->state);
	buddy = get_buddy(node);
	cur_state = buddy->state;
	new_state = order | FREE;

	if(GET_CURRENT_CPU_ZONE() != GET_ZONE_BY_PTR(node) || IS_NMI()){
		new_state |= STACK;
		__sync_bool_compare_and_swap(&buddy->state, cur_state, new_state);
		free_stack_push(&cpu_zones[buddy->zone].free_pools[order].free_stack, buddy);
	}
	else{
		new_state |= LIST;
		__sync_bool_compare_and_swap(&buddy->state, cur_state, new_state);
		free_list_insert(&cpu_zones[buddy->zone].free_pools[order].free_list, buddy);
	}
}



/*
 API for memory allocation.
 */
void* bd_xx_malloc(size_t byte){
	klbd_node_t *node = NULL;
	void *address = NULL;
	unsigned short order = 0;
	unsigned short cur_state, new_state;

    // check memory request size
    if( byte > MAX_ALLOCABLE_BYTES || byte > overall_memory_size)   
        return NULL;  

    // round to a proper size
    byte = upper_power_of_two(byte);
    if( byte < MIN_ALLOCABLE_BYTES ) 
        byte = MIN_ALLOCABLE_BYTES;

    order = log2_(byte/MIN_ALLOCABLE_BYTES) ;

  #ifdef TEST
    printf("allocating %lu\n", byte);
  #endif

    DISABLE_MIGRATION();
 
    for(unsigned char i=0; i<NUMBER_OF_CPUS; i++){
    	unsigned char z = (GET_CURRENT_CPU_ZONE() + i) % NUMBER_OF_CPUS;
    	
    	if(i>0){ 
    		printf("thisw should never happen %u %u %u\n", GET_CURRENT_CPU_ZONE(), i, z);
      		break;
      	}

      #ifdef TEST
    	printf("checking zone %u\n", z);
	  #endif
	  
	    for(unsigned char o=order; o<=MAX_ORDER; o++){
	      #ifdef TEST
    		printf("checking order %u stack\n", o);
    	  #endif
    		
    		do{
    			node = free_stack_pop(&cpu_zones[z].free_pools[o].free_stack);
    			if(node){
    				cur_state = node->state;
    				new_state = GET_ORDER(cur_state) | UNLINK | OCC;
    				if(!__sync_bool_compare_and_swap(&node->state, cur_state, new_state)) {
    					assert((false && "CHANGING STATE FROM A POPPED NODE FAILED"));
    					continue;
    				}
    				break;
    			}
    		}while(node);

			if(node){
				printf("stuff from stack\n");
			   	address = GET_ADDRESS_FROM_NODE(node);
			   	if(address == (void*)0xffff) printf("returning tail\n");
			}


    		if(!node){
    		  #ifdef TEST
    			printf("stack search failed\n");
		    	printf("checking order %u list\n", o);
			  #endif

				node = get_free_node(&cpu_zones[z],o);
    		  
//				if(!node) 
//    				printf("list search failed %u\n", o);
    		  #ifdef TEST
				if(!node) 
    			printf("list search failed\n");
			  #endif

    			if(node){
				   	address = GET_ADDRESS_FROM_NODE(node);
				   	if(address == (void*)0xffff) printf("returning tail\n");
    			}
    		}  
    		if(node) break;
    	}
		if(node) break;
    }


    if(!node) goto out;

   	address = GET_ADDRESS_FROM_NODE(node);
   	if(address == (void*)0xffff) printf("returning tail\n");


    while(GET_ORDER(node->state) > order){
    	split(node);
    }

   	address = GET_ADDRESS_FROM_NODE(node);
   	if(address == (void*)0xffff) printf("returning tail\n");

out:
   	if(per_cpu_data.cpu_zone == 0 && address != overall_memory){
   		printf("0 returning %u(%p) vs %u(%p)\n", node->idx,address, 0, overall_memory);
   	}
   	if(per_cpu_data.cpu_zone == 1 && address != (overall_memory+NUMBER_OF_LEAVES_PER_CPU_ZONE*MIN_ALLOCABLE_BYTES)){
   		printf("zone of %u:%u %u:%u\n", node->idx, node->zone, nodes[NUMBER_OF_LEAVES_PER_CPU_ZONE].idx, nodes[NUMBER_OF_LEAVES_PER_CPU_ZONE].zone);
   		printf("1 returning %u(%p) vs %u(%p)\n", node->idx,address, NUMBER_OF_LEAVES_PER_CPU_ZONE, overall_memory+NUMBER_OF_LEAVES_PER_CPU_ZONE*MIN_ALLOCABLE_BYTES);
   		exit(1);
   	}
   	ENABLE_MIGRATION();
    return address;
}

klbd_node_t* coalesce(klbd_node_t *node){
	klbd_node_t *buddy;
	bool skip;
	unsigned short order, i, cur_b_state, cur_state = node->state;
	order = GET_ORDER(cur_state);

	for(i=order;i < MAX_ORDER;i++){
		buddy = get_buddy(node);
		cur_b_state = buddy->state;
		skip = GET_AVAIL(cur_b_state) != FREE;
		skip = skip || GET_REACH(cur_b_state) == STACK;
		skip = skip || GET_ORDER(cur_b_state) != i;
		
		if(skip) break;

		if(__sync_bool_compare_and_swap(&buddy->state, cur_b_state, i | INV | LIST)){

			free_list_remove(buddy);
			__sync_bool_compare_and_swap(&buddy->state, i | INV | LIST, i | INV | UNLINK);
		}
		else break;
		if(is_left_buddy_from_idx(node->idx, i)) node = node;
		else node = buddy;
		__sync_fetch_and_add(&node->state, 1);
	}
	return node;
}


void bd_xx_free(void *ptr){
	if(!ptr) return;
	unsigned int idx = (((unsigned long long)ptr) - ((unsigned long long)overall_memory))/MIN_ALLOCABLE_BYTES;
	if(idx >= NUMBER_OF_LEAVES) {
		printf("ERROR %p %p %u\n", overall_memory, ptr, idx);
	}
	klbd_node_t *node = nodes + idx;
	unsigned short cur_state;
	bool is_unlinked;
	DISABLE_MIGRATION();

retry:
	cur_state = node->state;
	if(GET_CURRENT_CPU_ZONE() != GET_ZONE_BY_PTR(node) || IS_NMI()){
		is_unlinked = GET_REACH(cur_state) == UNLINK; 
		if(is_unlinked && __sync_bool_compare_and_swap(&node->state, cur_state, GET_ORDER(cur_state) | STACK | FREE ) ){
			free_stack_push(&cpu_zones[GET_ZONE_BY_PTR(node)].free_pools[GET_ORDER(cur_state)].free_stack, node);
		}
		else if(!is_unlinked && __sync_bool_compare_and_swap(&node->state, cur_state, GET_ORDER(cur_state) | LIST | FREE )){
			goto out;
		}
		else
			goto retry;
	}
	else{
		if(GET_REACH(cur_state) == LIST){
			free_list_remove(node);
			__sync_bool_compare_and_swap(&node->state, cur_state, GET_ORDER(cur_state) | UNLINK | OCC);
		}
		node = coalesce(node);
		cur_state = node->state;
		__sync_bool_compare_and_swap(&node->state, cur_state, GET_ORDER(cur_state) | LIST | FREE);
		free_list_insert(&cpu_zones[GET_ZONE_BY_PTR(node)].free_pools[GET_ORDER(cur_state)].free_list, node);
	}
out:
	ENABLE_MIGRATION();
}


