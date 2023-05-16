#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>

#include "utils.h"
#include "klbs.h"


#define AVAIL_OFFSET 8
#define REACH_OFFSET 10


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

#define GET_ZONE_BY_IDX(x) 				((x)/NUMBER_OF_LEAVES_PER_CPU_ZONE)
#define GET_ZONE_BY_PTR(x) 				((x)->zone)
#define GET_CURRENT_CPU_ZONE() 			(per_cpu_data.cpu_zone)
#define GET_ADDRESS_FROM_NODE(x)		((x)->address)
#define IS_NMI() 						(per_cpu_data.nmi >= 1)

#define DISABLE_MIGRATION() 			do{}while(0)
#define ENABLE_MIGRATION() 				do{}while(0)

#define DISABLE_INTERRUPT() 			do{\
__sync_fetch_and_add(&per_cpu_data.nmi, 1);\
}while(0)

#define ENABLE_INTERRUPT() 				do{\
__sync_fetch_and_add(&per_cpu_data.nmi, -1);\
}while(0)


__thread cpu_data_t per_cpu_data;

static unsigned int partecipants=0;
static volatile int init_phase                  = 0;

static void* volatile overall_memory            = NULL;
static unsigned long long overall_memory_size   = MEMORY_SIZE;

static klbd_node_t nodes[NUMBER_OF_LEAVES];
static cpu_zone_t cpu_zones[NUMBER_OF_CPUS];


static inline bool is_left_buddy_from_idx(unsigned int x, unsigned short o){
	return ((x >> o) & 1) == 0;
} 

static inline klbd_node_t* get_buddy(klbd_node_t* node){
	return nodes + (node->idx ^ (1<<GET_ORDER(node->state)));
}

bool free_list_insert(free_list_t *list, klbd_node_t *node);

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


	printf("\t| First pass on nodes...");
	fflush(stdout);
    for(i=0;i<NUMBER_OF_LEAVES;i++){
    	nodes[i].idx = i;
    	nodes[i].next = nodes[i].prev = NULL;
    	nodes[i].address = overall_memory + i*MIN_ALLOCABLE_BYTES;
		nodes[i].zone  = GET_ZONE_BY_IDX(i);
    	nodes[i].count = 0;
    	nodes[i].state = 0;
	}
	printf("Done\n");

    cur_order = MAX_ORDER;
	offset = 1 << cur_order;
	count_max = 0;
	total = 0;
	expected = NUMBER_OF_MAX_NODES;
	printf("\t| Second pass on nodes...");
	fflush(stdout);
	printf("state: %x",  MAX_ORDER | LIST | FREE);
    for(i=0;i<NUMBER_OF_LEAVES;i+=offset){
    	count_max++;
    	nodes[i].state = MAX_ORDER | LIST | FREE;
    	cur_zone = nodes[i].zone;
    	cpu_zone_t *zone = &cpu_zones[cur_zone];
    	free_pool_t *pool= &zone->free_pools[cur_order];
    	//free_list_insert(&pool->free_list, nodes+i);

		nodes[i].next = &pool->free_list.tail;
		nodes[i].prev = nodes[i].next->prev;
		nodes[i].next->prev = nodes+i;
		nodes[i].prev->next = nodes+i; 
	}
	printf("Done\n");

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
	  #ifdef TEST
		printf("FOUND %p state %x order %u free %u list %u\n", node, cur_state, GET_ORDER(cur_state), GET_AVAIL(cur_state)==FREE, GET_REACH(cur_state)==LIST);
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
				cur_state = new_state;
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

    // just on startup 
    if(per_cpu_data.cpu_zone == -1)  
        per_cpu_data.cpu_zone = __sync_fetch_and_add(&partecipants, 1);

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

    for(unsigned char z=0; z<1; z++){
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
    				if(!__sync_bool_compare_and_swap(&node->state, cur_state, new_state)) continue;
    				break;
    			}
    		}while(node); 
    		if(!node){
    		  #ifdef TEST
    			printf("stack search failed\n");
		    	printf("checking order %u list\n", o);
			  #endif
				node = get_free_node(&cpu_zones[z],o);
    		  #ifdef TEST
				if(!node) 
    			printf("list search failed\n");
			  #endif
    		}  
    		if(node) break;
    	}
    }

    if(!node) goto out;

    while(GET_ORDER(node->state) > order){
    	split(node);
    }

   	address = GET_ADDRESS_FROM_NODE(node);

out:
    ENABLE_MIGRATION();
    return address;
}

klbd_node_t* coalesce(klbd_node_t *node){
	return node;
}


void bd_xx_free(void *ptr){
	if(!ptr) return;
	unsigned int idx = (((unsigned long long)ptr) - ((unsigned long long)overall_memory))/MIN_ALLOCABLE_BYTES;
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


