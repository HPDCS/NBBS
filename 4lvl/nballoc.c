#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/wait.h>
#include "nballoc.h"
#include "utils.h"

#ifdef NUMA
	#include <numa.h>
	#include <numaif.h>
#include <sched.h>
#endif

/*********************************************	 
*	   MASKS FOR ACCESSING CONTAINER BITMAPS
*********************************************

Bitmap for container:
   47		 13 	12				11					10					9				8		 7  6  5  4  3  2  1  0	
  |------------|-----------|--------------------|--------------------|--------------|--------------|--|--|--|--|--|--|--|--|
  | RPEAT FOR  | OCCUPANCY | PENDING COALESCING | PENDING COALESCING | OCCUPANCY OF | OCCUPANCY OF |   ONE OCCUPANCY BIT   |
  | EACH LEAF  |		   | OPS ON LEFT  CHILD | OPS ON RIGHT CHILD | LEFT CHILD   | RIGHT CHILD  |   FOR EACH NOT LEAF   |
  |------------|-----------|--------------------|--------------------|--------------|--------------|--|--|--|--|--|--|--|--|

*/


//bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval)


#define LEAF_FULL_MASK (unsigned long long) (0x1FULL)
#define LEFT  ((unsigned long long)(0x2ULL))
#define RIGHT  ((unsigned long long)(0x1ULL))
#define COAL_LEFT  ((unsigned long long)(0x8ULL))
#define COAL_RIGHT  ((unsigned long long)(0x4ULL))
#define LOCK_LEAF ((unsigned long long)(0x13ULL))
#define TOTAL ((unsigned long long)(0xffffffffffffffffULL))
#define ABORT TOTAL
#define LOCK_LEAF_MASK (unsigned long long) (0x13ULL)
#define LOCK_NOT_LEAF_MASK (unsigned long long) (0x1ULL)

#define MASK_NODES_OCCUPIED (((0x10ULL) | (0x10ULL)<<5 | (0x10ULL)<<10 | (0x10ULL)<<15 | (0x10ULL)<<20 | (0x10ULL)<<25 | (0x10ULL)<<30 | (0x10ULL)<<35 | (0x10ULL)<<40 | (0x10ULL)<<45 | (0x10ULL)<<35) << 7)	

//questo è 8 quindi qua devo proprio vedere con le pos dal nodo.. parte da 1!
#define LEAF_START_POSITION (8) //la prima foglia del grappolo.. dipende dalla grandezza dei grappoli e si riferisce al node_container

#define IS_FREE(val, pos)  !IS_OCCUPIED(val, pos)
#define IS_LEAF(n) (((n)->container_pos) >= (LEAF_START_POSITION)) //attenzione: questo ti dice se il figlio è tra le posizione 8-15. Se sei foglia di un grappolo piccolo qua non lo vedi
#define IS_BUNCHROOT(n) ( (n->container_pos) == (1) )
#define BUNCHROOT(n) ((n->container->bunch_root))
#define LOCK_NOT_A_LEAF(val, pos)			((val)   | ( ((LOCK_NOT_LEAF_MASK) << ((pos-1)))))
#define UNLOCK_NOT_A_LEAF(val, pos)			((val)   & (~((LOCK_NOT_LEAF_MASK) << ((pos-1)))))
#define LOCK_A_LEAF(val, pos)				((val)   | ( ((LOCK_LEAF ) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))
#define UNLOCK_A_LEAF(val, pos)				((val)   & (~((LOCK_LEAF ) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))

#define BITMASK_LEAF(val, pos)				((LEAF_FULL_MASK)  & ( ((val) >> ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))	

//NOTA CHE QUESTE DEL COALESCE E LEFT/RIGHT SI DEVONO USA SOLO SULLE FOGLIE
#define COALESCE_LEFT(NODES, pos)	   	(unsigned long long) ((NODES) | ( ((COAL_LEFT ) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))
#define COALESCE_RIGHT(NODES, pos)	  	(unsigned long long) ((NODES) | ( ((COAL_RIGHT) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))
																																				   
#define CLEAN_LEFT_COALESCE(NODES,pos)  (unsigned long long) ((NODES) & (~((COAL_LEFT ) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))
#define CLEAN_RIGHT_COALESCE(NODES,pos) (unsigned long long) ((NODES) & (~((COAL_RIGHT) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))

#define OCCUPY_LEFT(NODES,pos)		  	(unsigned long long) ((NODES) | ( ((LEFT	  ) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))
#define OCCUPY_RIGHT(NODES,pos)		 	(unsigned long long) ((NODES) | ( ((RIGHT	  ) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))

#define CLEAN_LEFT(NODES, pos)		  	(unsigned long long) ((NODES) & (~((LEFT	  ) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))
#define CLEAN_RIGHT(NODES, pos)		 	(unsigned long long) ((NODES) & (~((RIGHT	  ) << ((LEAF_START_POSITION-1) + (5 * ((pos) - (LEAF_START_POSITION))))) ))

#define IS_OCCUPIED_LEFT(NODES, POS)	((NODES) == ( OCCUPY_LEFT(NODES,POS)))
#define IS_OCCUPIED_RIGHT(NODES, POS)   ((NODES) == (OCCUPY_RIGHT(NODES,POS)))

#define IS_COALESCING_LEFT(NODES, POS)  ((NODES) == ( COALESCE_LEFT(NODES,POS)))
#define IS_COALESCING_RIGHT(NODES, POS) ((NODES) == (COALESCE_RIGHT(NODES,POS)))

//se il fratello è in uso non devo risalire più a smarcare! se lo faccio sblocco il genitore di mio fratello che è occupato!
#define CHECK_BROTHER(parent, current, actual_value) \
		{ \
			if(\
			((&left(parent)) ==current && (!IS_ALLOCABLE(actual_value,(&right(parent))->container_pos))) || \
			((&right(parent))==current && (!IS_ALLOCABLE(actual_value, (&left(parent))->container_pos)))\
			){\
				exit=true;\
				break;\
			}\
		}

#define VAL_OF_NODE(n) ((unsigned long long) (n->container_pos<LEAF_START_POSITION ) ? ((n->container->nodes & (0x1ULL << (n->container_pos-1))) >> (n->container_pos-1)) : ((n->container->nodes & (LEAF_FULL_MASK << ((LEAF_START_POSITION-1) + (5 * ((n->container_pos-1) - (LEAF_START_POSITION-1))))))) >> ((LEAF_START_POSITION-1) + (5 * ((n->container_pos-1) - (LEAF_START_POSITION-1)))))

#define ROOT 		(tree[1])
#define left(n) 	(tree[((n->pos)*(2ULL))])
#define right(n) 	(tree[(((n->pos)*(2ULL))+(1ULL))])
#define left_index(n) 	(((n)->pos)*(2ULL))
#define right_index(n) 	(((n)->pos)*(2ULL)+(1ILL))
#define parent(n) 	(tree[/*(unsigned long long)*/(((n)->pos)/(2ULL))])
#define parent_index(n)  ((unsigned long long)(((n)->pos)/(2ULL)))

#define level(n) ((unsigned int) ( (overall_height) - (log2_(( (n)->mem_size) / (MIN_ALLOCABLE_BYTES )) )))

//PARAMETRIZZAZIONE
#define LEVEL_PER_CONTAINER 4


#ifdef NUMA
	bool allocate_in_remote_node = true; //make it possible to allocate in remote no. set to false if you don't want.
	node** overall_trees; //array di alberi! Un albero per ognu NUMA node
#endif
node* tree; //array che rappresenta l'albero, tree[0] è dummy! l'albero inizia a tree[1].
unsigned long long overall_memory_size;
unsigned long long overall_memory_pages;
unsigned int number_of_leaves;
unsigned int number_of_nodes; //questo non tiene presente che tree[0] è dummy! se qua c'è scritto 7 vuol dire che ci sono 7 nodi UTILIZZABILI
#ifdef NUMA
	void** overall_memory; //array of pointers. One pointer for each numa NODE
#else
	void* overall_memory;
#endif
node trying;
unsigned int failed_at_node;
unsigned int overall_height;

#ifdef NUMA
	node_container** overall_containers; //Uno per ogni albero (uno per ogni nodo)
#endif
node_container* containers; //array di container In Numa questo sarà uno specifico tree (di base tree[getMyNUMANode])

unsigned int first_available_container = 0;
node* upper_bound;

int number_of_processes;
#ifdef NUMA
int number_of_numa_nodes;
#endif

#ifdef DEBUG
unsigned long long *node_allocated, *size_allocated;
#endif

void init_node(node* n);
void init_tree(unsigned long long number_of_nodes);
void init(unsigned long long memory);
void end();
//void print(node* n);
bool alloc(node* n);
void marca(node* n);
bool IS_OCCUPIED(unsigned long long, unsigned);
#ifdef NUMA
void do_move(void * buffer, unsigned int target_node, unsigned int n_pages);
#endif


bool check_parent(node* n);
void smarca_(node* n);
void smarca(node* n){
#ifdef NUMA
	tree = overall_trees[n->numa_node];
	containers = overall_containers[n->numa_node];
#endif
	upper_bound = &ROOT;
	smarca_(n);
}

//void print_in_profondita(node*);
//void print_in_ampiezza();
void free_node_(node* n);

/*Queste funzioni sono esposte all'utente*/
node* request_memory(unsigned int pages);
void free_node(node* n){
#ifdef NUMA
	tree = overall_trees[n->numa_node];
	containers = overall_containers[n->numa_node];
#endif
	upper_bound = &ROOT;
	free_node_(n);
#ifdef DEBUG
	__sync_fetch_and_add(node_allocated,-1);
	__sync_fetch_and_add(size_allocated,-(n->mem_size));
#endif
}



//MARK: INIT


/*
 Questa funzione inizializza l'albero. Non il nodo dummy (tree[0])
 @param number_of_nodes: the number of nodes.
 */
void init_tree(unsigned long long number_of_nodes){
	unsigned long long i=0;
#ifdef NUMA
	int j = -1;
next_node_label:
//	puts("giro");
	first_available_container = 0;
	j++;
	tree = overall_trees[j];
	containers = overall_containers[j];
#endif

	ROOT.mem_start = 0;
	ROOT.mem_size = overall_memory_size;
	ROOT.pos = 1ULL;
	ROOT.container = &containers[first_available_container++];
	ROOT.container_pos = 1ULL;
	ROOT.container->bunch_root = &ROOT;
    ROOT.container->nodes = 0ULL;
#ifdef NUMA
	ROOT.numa_node = j;
#endif
	
	//init_node(ROOT);
	for(i=2;i<=number_of_nodes;i++){
#ifdef AUDIT
		printf("%d\n", i);
#endif
		tree[i].pos = i;
		node parent = parent(&tree[i]);
		tree[i].mem_size = (parent.mem_size) / 2;
#ifdef NUMA
		tree[i].numa_node = j;
#endif
		
		if(level(&tree[i])%LEVEL_PER_CONTAINER==1){
			tree[i].container = &containers[first_available_container++];
			tree[i].container_pos = 1ULL;
			tree[i].container->nodes = 0ULL;
			tree[i].container->bunch_root = &tree[i];
		}
		else{
			tree[i].container = parent.container;
			tree[i].container_pos = (parent.container_pos*2)+(1&(left_index(&parent)!=i));
			//if(left_index(&parent)==i)
			//	tree[i].container_pos = parent.container_pos*2;
			//else
			//	tree[i].container_pos = (parent.container_pos*2)+1;
		}
			
		
		if(left_index(&parent)==i)
			tree[i].mem_start = parent.mem_start;
		
		else
			tree[i].mem_start = parent.mem_start + tree[i].mem_size;
		
	   
	}

#ifdef NUMA
	if(j != number_of_numa_nodes-1)
		goto next_node_label;
#endif
	
	
#ifdef AUDIT
	for(i=1;i<=number_of_nodes;i++){
		printf("I am %u, my container is %ld and my position is %u level is %u\n", tree[i].pos, tree[i].container - containers, tree[i].container_pos, level(&tree[i]));
	}
	exit(0);
	
#endif
}

#ifdef NUMA
/*
	Funzione che effettua la move_pages (portando le pagine sul nodo target_node) delle n_pages che iniziano da buffer.
	@param buffer: puntatore all'inizio della memoria da migrare
	@param target_node: nodo su cui spostare la memoria
	@param n_pages: numero di pagine da spostare partendo da buffer

 */
void do_move(void * buffer, unsigned int target_node, unsigned int n_pages) {
#ifdef NUMA_AUDIT
	printf("******MIGRATE TO %u\n", target_node);
#endif
	void* pages[n_pages];
	int nodes[n_pages];
	int status[n_pages];
	int i;
	int returned;
	for (i = 0; i < n_pages; i++){
		pages[i] = buffer + (i * PAGE_SIZE);
		nodes[i] = target_node;
		status[i] = -1;
	}
	for (i = 0; i < n_pages; i++){
		//INITIALIZE
		*(long*)pages[i] = 0;
	}

	returned = move_pages(0, n_pages, (void **)pages, nodes, status, MPOL_MF_MOVE);
	if(returned != 0)
		puts("something was wrong in move_pages..");
#ifdef NUMA_AUDIT
	printf("returned is %d, errno is%d\n", returned, errno);
	for (i = 0; i < n_pages; i++){
		printf("status[%d]=%d\n", i, status[i]);
	}
#endif
}
#endif


#ifdef NUMA
/*
	@param levels:  Numero di livelli dell'albero - NUMA version
 */
void init(unsigned long long levels){
	
#ifdef NUMA_AUDIT
	int numa_node = -1;
#endif
	
	number_of_nodes = (1<<levels) -1;
	printf("NUMA %d\n", number_of_nodes);
	
	number_of_leaves = (1<< (levels-1));
	overall_memory_size = MIN_ALLOCABLE_BYTES * number_of_leaves;
	printf("number_of_leaves = %u, overall_memory_size=%llu\n", number_of_leaves, overall_memory_size);
	
	int i;
	int num_pages;
	//se abbiamo numa overall_memory è un array di puntatori. Ognuno è presente in un certo nodo.
	overall_memory = mmap(NULL, number_of_numa_nodes * sizeof(void*), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	if(overall_memory==MAP_FAILED)
		abort();
	
	for(i=0;i<number_of_numa_nodes;i++){
		//overall_memory[i] = numa_alloc_onnode(overall_memory_size, i);
		overall_memory[i] = mmap(NULL, overall_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		num_pages = overall_memory_size / PAGE_SIZE;
		if((overall_memory_size % PAGE_SIZE) != 0)
			num_pages++;
		do_move((void*)overall_memory[i], i, num_pages);
		if(overall_memory[i]==NULL)
			abort();
#ifdef NUMA_AUDIT
		numa_node = -1;
		get_mempolicy(&numa_node, NULL, 0, (void*)overall_memory[i], MPOL_F_NODE | MPOL_F_ADDR);
		printf("overall_memory[i] (expected NODE) is %d, numa_node is %d\n", i, numa_node);
#endif
	}
	
	overall_height = levels;
	
	
	//se abbiamo numa tree è un array  e containers, sono array, ognuno riferito ad un certo numa node
	overall_trees = mmap(NULL, number_of_numa_nodes * sizeof(node*), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	if(overall_trees==MAP_FAILED)
		abort();
	
	overall_containers = mmap(NULL, number_of_numa_nodes * sizeof(node_container*), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	if(overall_containers==MAP_FAILED)
		abort();
	for(i=0;i<number_of_numa_nodes;i++){
		overall_trees[i] = mmap(NULL,(1+number_of_nodes)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		overall_containers[i] = mmap(NULL,(number_of_nodes-1)*sizeof(node_container),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		
		num_pages = ((1+number_of_nodes)*sizeof(node)) / PAGE_SIZE;
		if((((1+number_of_nodes)*sizeof(node)) % PAGE_SIZE) != 0)
			num_pages++;
		do_move((void*)overall_trees[i], i, num_pages);
		
		num_pages = ((number_of_nodes-1)*sizeof(node_container))/PAGE_SIZE;
		if((((number_of_nodes-1)*sizeof(node_container)) % PAGE_SIZE) != 0)
			num_pages++;
		do_move((void*)overall_containers[i], i, num_pages);
		
		if(overall_trees[i]==NULL || overall_containers[i]==NULL)
			abort();
		
		
		
#ifdef NUMA_AUDIT
		numa_node = -1;
		get_mempolicy(&numa_node, NULL, 0, (void*)overall_trees[i], MPOL_F_NODE | MPOL_F_ADDR);
		printf("overall_trees[i] (expected NODE) is %d, numa_node is %d\n", i, numa_node);
		numa_node = -1;
		get_mempolicy(&numa_node, NULL, 0, (void*)overall_containers[i], MPOL_F_NODE | MPOL_F_ADDR);
		printf("overall_containers[i] (expected NODE) is %d, numa_node is %d\n", i, numa_node);
#endif
	}
	
	init_tree(number_of_nodes);
	puts("NUMA init complete");
	
#ifdef DEBUG
	node_allocated = mmap(NULL, sizeof(unsigned long long), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	size_allocated = mmap(NULL, sizeof(unsigned long long), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	  
	__sync_fetch_and_and(node_allocated,0);
	__sync_fetch_and_and(size_allocated,0);
	
	printf("Debug mode: ON\n");
#endif
	
}
#else


/*
	@param pages: pagine richieste. Questo sarà il valore della radice - NON NUMA VERSION
 */
void init(unsigned long long levels){
	number_of_nodes = (1<<levels) - 1;
	printf("NON NUMA: %d\n", number_of_nodes);
	
	number_of_leaves = (1<< (levels-1));
	overall_memory_size = MIN_ALLOCABLE_BYTES * number_of_leaves;
	printf("number_of_leaves = %u, overall_memory_size=%llu\n", number_of_leaves, overall_memory_size);
	
	overall_memory = mmap(NULL, overall_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
//   printf("overall memory is %p of size %llu\n", overall_memory, overall_memory_size);
	
	if(overall_memory==MAP_FAILED){
		puts("1");
		abort();
	}
	
	overall_height = levels;

	tree = mmap(NULL,(1+number_of_nodes)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	if(tree==MAP_FAILED){
		puts("2");
		abort();
	}
	
	containers = mmap(NULL,(number_of_nodes-1)*sizeof(node_container), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if(containers==MAP_FAILED){
		puts("3");
		abort();
	}
	
	init_tree(number_of_nodes);
	
#ifdef DEBUG
	node_allocated = mmap(NULL, sizeof(unsigned long long), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	size_allocated = mmap(NULL, sizeof(unsigned long long), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	  
	__sync_fetch_and_and(node_allocated,0);
	__sync_fetch_and_and(size_allocated,0);
	
	printf("Debug mode: ON\n");
#endif
	
	printf("UMA Init complete\n");
	printf("\t Total Memory = %llu\n", overall_memory_size);
	printf("\t Levels = %u\n", overall_height);
	printf("\t Leaves = %u\n", (number_of_nodes+1)/2);
	printf("\t Nodes = %u\n", number_of_nodes);
	printf("\t Containers = %u\n", first_available_container);
	printf("\t Min size %u at level %u\n", MIN_ALLOCABLE_BYTES, overall_height);
	printf("\t Max size %u at level %u\n", MAX_ALLOCABLE_BYTE, overall_height - log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES));
	
}
#endif

/*
	Funzione finale che nell'ordine:
	1) libera la memoria assegnabile
	3) libera l'array che memorizzava l'albero
 */
void end(){
#ifdef NUMA
	int i;
	for(i=0;i<number_of_numa_nodes;i++){
		free(overall_memory[i]);
		free(overall_containers[i]);
		free(overall_trees[i]);
	}
	free(overall_memory);
	free(overall_containers);
	free(overall_trees);
	
#else
	free(overall_memory);
	//free_nodes(ROOT);
	free(tree);
#endif
}


//MARK: ALLOCAZIONE

/*
	Questa funziona controlla se il nodo in posizione container_pos è libero nella maschera dei nodi rappresentata da val. Per la semantica dell'algoritmo un nodo è allocabile se e solo se esso è completamente libero, i.e. il nodo è completamento 0. Per questo motivo per le foglie il check è fatto con 0x1F
	@param val: il container nel quale il nodo si trova
	@param pos: il container_pos del nodo che vogliamo controllare
 
 */
bool IS_ALLOCABLE(unsigned long long val, unsigned int pos){
	bool ret = false;
	if(pos<LEAF_START_POSITION){
		//non è una foglia
		ret =  ( (val & ((LOCK_NOT_LEAF_MASK) << (pos-1))) != 0);
		return !ret;
	}
	else{
		ret = val & (( (LEAF_FULL_MASK) << (LEAF_START_POSITION-1)) << (5* (pos-(LEAF_START_POSITION))));
		return !ret;
	}
}

//non può essere una macro perchè mi servono gli if e questa viene chiamata all'interno di if
/*
 Questa funziona controlla se il nodo in posizione container_pos è TOTALMENTE occupato (se si tratta di una foglia) oppure se è parzialmente o totalmente occupato se non è una foglia, nella maschera dei nodi rappresentata da val. Ricordo che per la semantica dell'algoritmo una foglia è totalmente occupata se e solo ha il quinto bit settato.
 @param val: il container nel quale il nodo si trova
 @param pos: il container_pos del nodo che vogliamo controllare
 
 */
bool IS_OCCUPIED(unsigned long long val, unsigned int pos){
	bool ret = false;
	if(pos<LEAF_START_POSITION){
		//non è una foglia
		ret =  ( (val & ((LOCK_NOT_LEAF_MASK) << (pos-1))) != 0);
		return ret;
	}
	else{
		//è una foglia
		ret = val & (( (LOCK_NOT_LEAF_MASK) << (LEAF_START_POSITION-2)) << (5* (pos-(LEAF_START_POSITION-1))));
		return ret;
	}
}



/*
 Funzione di malloc richiesta dall'utente.
 @param pages: memoria richiesta dall'utente
 @return l'indirizzo di memoria del nodo utilizzato per soddisfare la richiesta; NULL in caso di fallimento
 */
node* request_memory(unsigned int byte){
	bool restarted = false; 
	unsigned int started_at, actual, starting_node, last_node;
	
	if(byte>MAX_ALLOCABLE_BYTE)
		return NULL;
		byte = upper_power_of_two(byte);
	if(byte<MIN_ALLOCABLE_BYTES)
		byte = MIN_ALLOCABLE_BYTES;
	
	starting_node = overall_memory_size / byte; //first node for this level
	last_node = left_index(&tree[starting_node])-1; //last node for this level
		
#ifdef NUMA
	//inizia a cercare nell'albero relativo al nodo numa in cui sto girando
	int starting_numa_node = numa_node_of_cpu(sched_getcpu());
	int trying_on_numa_node =  starting_numa_node;
try_on_a_new_numa_node_label:
	tree = overall_trees[trying_on_numa_node];
	containers = overall_containers[trying_on_numa_node];
#endif
	
	//actual è il posto in cui iniziare a cercare
	actual = started_at = starting_node + (myid) * ((last_node - starting_node + 1)/number_of_processes);
	//printf("myid:%d starting:%u\n", myid, actual-512);
   
   
	//quando faccio un giro intero ritorno NULL
	do{
		if(alloc(&tree[actual])==true){
#ifdef DEBUG
			__sync_fetch_and_add(node_allocated,1);
			__sync_fetch_and_add(size_allocated,byte);
#endif
			return &tree[actual];
		}
		
		if(failed_at_node==1){ // il buddy è pieno
#ifdef NUMA
			if(allocate_in_remote_node && ((trying_on_numa_node+1)%number_of_numa_nodes) != starting_numa_node){
				trying_on_numa_node = (trying_on_numa_node+1)%number_of_numa_nodes;
				goto try_on_a_new_numa_node_label;
			}
			else
				return NULL;
#else
			return NULL;
#endif
		}
		
		//Questo serve per evitare tutto il sottoalbero in cui ho fallito
		actual=(failed_at_node+1)* (1<<( level(&tree[actual]) - level(&tree[failed_at_node])));
		
		
		if(actual>last_node){ //se ho sforato riparto dal primo utile, se il primo era quello da cui avevo iniziato esco al controllo del while
			actual=starting_node;
			restarted = true;
		}
		
	}while(restarted == false || actual < started_at);
	
#ifdef NUMA
	if(allocate_in_remote_node && ((trying_on_numa_node+1)%number_of_numa_nodes) != starting_numa_node){
		trying_on_numa_node = (trying_on_numa_node+1)%number_of_numa_nodes;
		goto try_on_a_new_numa_node_label;
	}
	else
		return NULL;
#else
	return NULL;
#endif
}

/*
	Questa è una funzione di help per la alloc. Occupa tutti i discendenti del nodo n presenti nello stesso grappolo. NB questa funzione modifica solo new_val; non fa CAS, la modifica deve essere apportata dal chiamante.
	@param n: il nodo (OCCUPATO) a cui occupare i discendenti
	@param new_val: sarebbe  n->container->nodes da modificare
 
 */
unsigned long long occupa_discendenti(node* n, unsigned long long new_val){
	//se l'ultimo grappolo non ha tutti i nodi che deve avere (tipo ha solo due livelli). QUesto quando andiamo a regime non dovrebbe succedere perchè questa situazione la evitiamo (inutile avere un grappolo monco.. il numero di cas è lo stesso
	if(left_index(n)>=number_of_nodes)//MAURO: non sono sicuro sia corretto...
		return new_val;
	if(!IS_LEAF(&left(n))){
		new_val = LOCK_NOT_A_LEAF(new_val, left(n).container_pos);
		new_val = LOCK_NOT_A_LEAF(new_val, right(n).container_pos);
		new_val = occupa_discendenti(&left(n), new_val);
		new_val = occupa_discendenti(&right(n), new_val);//MAURO: vediamo se si può eliminare la ricorsione
	}
	else{
		new_val = LOCK_A_LEAF(new_val, left(n).container_pos);
		new_val = LOCK_A_LEAF(new_val, right(n).container_pos);
	}
	return new_val;
}

/*
 Prova ad allocare un DATO nodo.
 Con questa allocazione abbiamo che se un generico nodo è occupato, è occupato tutto il suo ramo
 nel grappolo.. cioè se per esempio è allocato uno qualsiasi 1,2,5,10 questi 5 saranno tutti e 5 flaggati come occupati.
 Vengono anche flaggati tutti i figli nello stesso grappolo ma NON i figli nei grappoli sottostanti.
 Side effect: se fallisce subito, prima di chiamare la check_parent la variabile globale failed_at_node assumerà il valore n->pos
 @param n: nodo presunto libero (potrebbe essere diventato occupato concorrentemente)
 @return true se l'allocazione riesce, false altrimenti
 
 */
bool alloc(node* n){
	unsigned long long old_val, new_val;
	node *current, *parent, *root_grappolo;
	
	do{
		new_val = old_val = n->container->nodes;
		if(!IS_ALLOCABLE(new_val, n->container_pos)){
			failed_at_node = n->pos;
			return false;
		}
		current = n;
		root_grappolo = BUNCHROOT(n);
		parent = &parent(n);
		//marco i genitori in questo grappolo Sicuramente non è una foglia visto che parto da un genitore
		while(current!=root_grappolo){
#ifdef NUMA_AUDIT
			printf("n is %u current is %u and root_grappolo is %u\n", n->pos, current->pos, root_grappolo->pos);
#endif
			new_val = LOCK_NOT_A_LEAF(new_val, parent->container_pos); //MAURO: <--SETTA TUTTE LE NON FOGLIE COME PROPRIE...in sostanza ogni allocazioned di un grappolo blocca tutto il grappolo?
			current = parent;
			parent = &parent(current);
		}
		//marco n se è una foglia
		if(IS_LEAF(n)){
			new_val = LOCK_A_LEAF(new_val, n->container_pos);
		}
		//marco n ed i suoi figli se n non è una foglia
		else{
			//marco n
			new_val = LOCK_NOT_A_LEAF(new_val,n->container_pos);
			//marco i suoi discendenti(se questo nodo ha figli). nota che se ha figli gli ha sicuro nello stesso grappolo visto che non è foglia dal controllo precedente
			if(left_index(n)<number_of_nodes) //MAURO: lo rifà in occupa discendenti
				new_val = occupa_discendenti(n, new_val);
		}
#ifdef DEBUG
		if(level(n) != overall_height){
			printf("Sto allocando una non-foglia\n");
			abort();
		}
#endif
	}while(!__sync_bool_compare_and_swap(&n->container->nodes, old_val, new_val));
	//Se n appartiene al grappolo della radice
	if(n->container->bunch_root==&ROOT){
		return true;
	}
	else if(check_parent(BUNCHROOT(n))){
		return true;
	}
	else{
		free_node_(n);
		return false;
	}
}


/*
 Questa funzione si preoccupa di marcare il bit relativo al sottoalbero del blocco che ho allocato in tutti i suoi antenati.
 Questa funziona ritorna true se e solo se riesce a marcare fino alla radice. La funzione fallisce se e solo se si imbatte in un nodo occupato (ricordando che se un generico nodo è occupato, allora la foglia ad esso relativa (il nodo stesso o un suo discendente) avrà il falore 0x13)
 La funzione si preoccupa inoltre di cancellare l'eventuale bit di coalescing.
 Side effect: se fallisce la variabile globale failed_at_node assume il valore del nodo dove la ricorsione è fallita (TODO: occhio, fallisco sulla foglia, magari il nodo bloccato era il padre, non ho modo di saperlo, rischio di sbattare nuovamente su questo nodo)
 @param n: nodo da cui iniziare la risalita (che è già marcato totalmente o parzialmente). Per costruzione della alloc, n è per forza un nodo radice di un grappolo generico (ma sicuramente non la radice)
 @return true se la funzione riesce a marcare tutti i nodi fino alla radice; false altrimenti.
 
 */
bool check_parent(node* n){
	node* parent = &parent(n);
#ifdef DEBUG
	node *parent_fix = &parent(n);
#endif
	node* root_grappolo = parent->container->bunch_root;
	upper_bound = n; //per costruione upper bound sarà quindi la radice dell'ultimo bunch da liberare in caso di fallimento. E' quello precedente al bunch dove lavoriamo adesso perchè se adesso falliamo, non apporteremo le modifiche
	unsigned long long new_val, old_val;
	do{
		parent = &parent(n);
		new_val = old_val = parent->container->nodes;
		if(IS_OCCUPIED(parent->container->nodes, parent->container_pos)){
			failed_at_node = parent->pos;
			return false;
		}
#ifdef DEBUG
		if((parent->container_pos)<1 || (parent->container_pos)>15){
			printf("Posizione container errata %u\n", (parent->container_pos));
			abort();
		}
#endif		
		
		
		if(&left(parent)==n){
			new_val = CLEAN_LEFT_COALESCE(new_val, parent->container_pos);
			new_val = OCCUPY_LEFT(new_val, parent->container_pos);
		}else{
			new_val = CLEAN_RIGHT_COALESCE(new_val, parent->container_pos);
			new_val = OCCUPY_RIGHT(new_val, parent->container_pos);
		}
#ifdef DEBUG
		if(new_val & MASK_NODES_OCCUPIED){
			printf("PRIMA: ---Check_parent: Sto scrivendo 1 in un nodo non-foglia P:%p(%p) L:%p(%p) R:%p(%p) N:%p(%p)(da_root %p) \n",parent,parent-tree,  &left(parent), &left(parent)-tree, &right(parent),&right(parent)-tree, n ,n-parent,n-tree);
			printf("Sono al livello %d, indice container(%u)\n", level(parent_fix),parent_fix->container_pos);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", new_val, old_val);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", BITMASK_LEAF(new_val, parent_fix->container_pos), BITMASK_LEAF(old_val, parent_fix->container_pos));
			abort();
		}
		if(BITMASK_LEAF(new_val, parent_fix->container_pos) & 0x10ULL){
			printf("Check_parent: Sto scrivendo 1 in un nodo non-foglia\n");
			printf("Sono al livello %d, indice container(%u)\n", level(parent_fix),parent_fix->container_pos);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", new_val, old_val);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", BITMASK_LEAF(new_val, parent_fix->container_pos), BITMASK_LEAF(old_val, parent_fix->container_pos));
			abort();
		}
#endif
		
		new_val = LOCK_NOT_A_LEAF(new_val, parent(parent).container_pos);
		parent = &parent(parent);
		new_val = LOCK_NOT_A_LEAF(new_val, parent(parent).container_pos);
		new_val = LOCK_NOT_A_LEAF(new_val, root_grappolo->container_pos);
		
#ifdef DEBUG
		if(new_val & MASK_NODES_OCCUPIED){
			printf("---Check_parent: Sto scrivendo 1 in un nodo non-foglia %u\n", &left(parent)==n);
			printf("Sono al livello %d, indice container(%u)\n", level(parent_fix),parent_fix->container_pos);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", new_val, old_val);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", BITMASK_LEAF(new_val, parent_fix->container_pos), BITMASK_LEAF(old_val, parent_fix->container_pos));
			abort();
		}
		if(BITMASK_LEAF(new_val, parent_fix->container_pos) & 0x10ULL){
			printf("Check_parent: Sto scrivendo 1 in un nodo non-foglia\n");
			printf("Sono al livello %d, indice container(%u)\n", level(parent_fix),parent_fix->container_pos);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", new_val, old_val);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", BITMASK_LEAF(new_val, parent_fix->container_pos), BITMASK_LEAF(old_val, parent_fix->container_pos));
			abort();
		}
#endif
	
		
	}while(!__sync_bool_compare_and_swap(&(parent(n).container->nodes),old_val,new_val));
	
	if(root_grappolo==&ROOT)
		return true;
	
	return check_parent(root_grappolo);
}

//MARK: FREE

/*
 Duale della occupa_discendenti
 Questa è una funzione di help per la free. Libera tutti i discendenti del nodo n presenti nello stesso grappolo. NB questa funzione modifica solo new_val; non fa CAS, la modifica deve essere apportata dal chiamante
 @param n: il nodo a cui liberare i discendenti
 @param new_val: sarebbe  n->container->nodes da modificare
 
 */
unsigned long long libera_discendenti(node* n, unsigned long long new_val){
	
	
	//se l'ultimo grappolo non ha tutti i nodi che deve avere (tipo ha solo due livelli). QUesto quando andiamo a regime non dovrebbe succedere perchè questa situazione la evitiamo (inutile avere un grappolo monco.. il numero di cas è lo stesso
	if(left_index(n)>=number_of_nodes)
		return new_val;
	
	if(!IS_LEAF(&left(n))){
		new_val = UNLOCK_NOT_A_LEAF(new_val, left(n).container_pos);
		new_val = UNLOCK_NOT_A_LEAF(new_val, right(n).container_pos);
		new_val = libera_discendenti(&left(n), new_val);
		new_val = libera_discendenti(&right(n), new_val);
	}
	else{
		new_val = UNLOCK_A_LEAF(new_val,left(n).container_pos);
		new_val = UNLOCK_A_LEAF(new_val,right(n).container_pos);
	}
	return new_val;
}

/*
 Questa funzione fa la free_node da n al nodo rappresentato dalla variabile globale upper_bound.
 Questa funzione potrebbe essere chiamata sia per liberare un nodo occupato, sia per annullare le modifiche apportate da una allocazione che ha fallito (in quel caso upper_bound non è la root ma è il nodo in cui la alloc ha fallito).
 upper_bound è l'ultimo nodo da smarcare ed è, per costruzione, la radice di un grappolo (LO ABBIAMO MARCATO NOI QUINDI LO DOBBIAMO SMARCARE!).
 La funzione è divisa in 3 step.
	1) vengono marcati i grappoli antecedenti come in coalescing (funzione marca)
	2) Viene liberato il nodo n ed il relativo grappolo
	3) vengo smarcati i grappoli antecedenti (funzione smarca)
 @param n è un nodo generico ma per come facciamo qui la allocazione tutto il suo ramo è marcato.
*/
void free_node_(node* n){
	unsigned long long old_val, new_val;
	node *current, *parent;
	bool exit = false;
	
#ifdef NUMA
	tree = overall_trees[n->numa_node];
	containers = overall_containers[n->numa_node];
#endif
	
#ifdef DEBUG
		if(level(n) != overall_height){
			printf("Sto de-allocando una non-foglia\n");
			abort();
		}
#endif

	if(BUNCHROOT(n) != upper_bound)
		marca(BUNCHROOT(n));
	//LIBERA TUTTO CIO CHE RIGUARDA IL NODO E I SUOI DISCENDENTI ALL INTERNO DEL GRAPPOLO. ATTENZIONE AI GENITORI ALL'INTERNO DEL GRAPPOLO (p.es. se il fratello
	//è marcato; non smarcare il padre). Nota che fino a smarca non mi interessa di upper_bound
	do{
		exit=false;
		current = n;
		parent = &parent(current);

		old_val = new_val = current->container->nodes;
		while(current!=BUNCHROOT(n)){
			//questo termina il ciclo se il fratello è occupato e setta exit = true
			CHECK_BROTHER(parent,current, new_val);
			
			//qua ci arriviamo solo se il fratello è libero
			new_val = UNLOCK_NOT_A_LEAF(new_val, parent->container_pos);
			current = parent;
			parent = &parent(current);
		}
		if(!IS_LEAF(n) && left_index(n)<=number_of_nodes){ //se non è foglia (nel senso che non è tra le posizione 8-15 e se i figli esistono.
			new_val = libera_discendenti(n,new_val);
		}
		if(IS_LEAF(n))
			new_val = UNLOCK_A_LEAF(new_val, n->container_pos);
		else
			new_val = UNLOCK_NOT_A_LEAF(new_val, n->container_pos);
		
	}while(!__sync_bool_compare_and_swap(&n->container->nodes,old_val, new_val));
	//find_the_bug(2);

	if(BUNCHROOT(n) != upper_bound && !exit)
		smarca_(BUNCHROOT(n));
}

/*
 
 Funzione ausiliaria a free_node_ (in particolare al primo step della free). In questa fase la free vuole marcare tutti i nodi dei sottoalberi interessati dalla free come "coalescing". Qui n è la radice di un grappolo. Bisogna mettere il bit di coalescing al padre (che quindi sarà una foglia con 5 bit). Nota che questa funzione non ha ragione di esistere se il nodo da liberare è nel grappolo di upper_bound.
 @param n è la radice di un grappolo. Bisogna settare in coalescing il padre.
 @return il valore precedente con un singolo nodo marcato come "coalescing"
 
 */
void marca(node* n){
	node* parent = &parent(n);
	unsigned long long old_val,new_val;
	
#ifdef DEBUG
	node *parent_fix = &parent(n);
#endif
	do{
		new_val = old_val = parent->container->nodes;
		if (&(left(parent))==n){
			new_val = COALESCE_LEFT(new_val, parent->container_pos);
		}
		else
			new_val = COALESCE_RIGHT(new_val, parent->container_pos);
			
#ifdef DEBUG
		if(new_val & MASK_NODES_OCCUPIED){
			printf("---Marca: Sto scrivendo 1 in un nodo non-foglia\n");
			printf("Sono al livello %d, indice container(%u)\n", level(parent_fix),parent_fix->container_pos);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", new_val, old_val);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", BITMASK_LEAF(new_val, parent_fix->container_pos), BITMASK_LEAF(old_val, parent_fix->container_pos));
			abort();
		}
		if(BITMASK_LEAF(new_val, parent_fix->container_pos) & 0x10ULL){
			printf("Marca: Sto scrivendo 1 in un nodo non-foglia\n");
			printf("Sono al livello %d, indice container(%u)\n", level(parent_fix),parent_fix->container_pos);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", new_val, old_val);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", BITMASK_LEAF(new_val, parent_fix->container_pos), BITMASK_LEAF(old_val, parent_fix->container_pos));
			abort();
		}
#endif
		
	}while(!__sync_bool_compare_and_swap(&parent->container->nodes, old_val, new_val));
	//find_the_bug(4);
	if(BUNCHROOT(parent)!=upper_bound)
		marca(BUNCHROOT(parent));
}



/*
 Questa funziona leva il bit di coalesce e libera il sottoramo (fino all'upper bound) ove necessario. Nota che potrebbe terminare lasciando alcuni bit di coalescing posti ad 1. Per i dettagli si guardi la documentazione. Upper bound è l'ultimo nodo da smarcare. n è la radice di un grappolo. Va pulito il coalescing bit al padre di n (che è su un altro grappolo). Quando inizio devo essere certo che parent(n) abbia il coalescing bit settato ad 1.
		La funzione termina prima se il genitore del nodo che voglio liberare ha l'altro nodo occupato (sarebbe un errore liberarlo)
 @param n: n è la radice di un grappolo (BISOGNA SMARCARE DAL PADRE)
 
 */
void smarca_(node* n){
	bool exit=false;
	node* current;
	node* parent;
	unsigned long long old_val, new_val;
#ifdef DEBUG
	node *parent_fix = &parent(n);
#endif
	
	do{
		exit = false;
		current = n;
		parent = &parent(n);
		
		old_val = new_val = parent->container->nodes;
		if(&(left(parent))==current){
			if(!IS_COALESCING_LEFT(new_val, parent->container_pos)) //qualcuno l'ha già pulito
				return;
			new_val = CLEAN_LEFT_COALESCE(new_val, parent ->container_pos); //lo facciamo noi
			new_val = CLEAN_LEFT(new_val, parent -> container_pos);
			//SE il fratello è occupato vai alla CAS
			if(IS_OCCUPIED_RIGHT(new_val, parent -> container_pos))
				continue;
		}
		if(&(right(parent))==current){
			if(!IS_COALESCING_RIGHT(new_val, parent->container_pos)) //qualcuno l'ha già pulito
				return;
			new_val = CLEAN_RIGHT_COALESCE(new_val, parent ->container_pos); //lo facciamo noi
			new_val = CLEAN_RIGHT(new_val, parent -> container_pos);
			//SE il fratello è occupato vai alla CAS
			if(IS_OCCUPIED_LEFT(new_val, parent -> container_pos))
				continue;
		}
		
		
		//ORA VADO A SMARCARE TUTTI GLI ALTRI NODI NON FOGLIA DI QUESTO GRAPPOLO
		current = parent;
		parent = &parent(current);
		while(current!=BUNCHROOT(current)){
			//questo termina il ciclo se il fratello è occupato e setta exit = true
			CHECK_BROTHER(parent, current, new_val);
			
			new_val = UNLOCK_NOT_A_LEAF(new_val, parent->container_pos);
			current = parent;
			parent = &(parent(current));
		}
		
#ifdef DEBUG
		if(new_val & MASK_NODES_OCCUPIED){
			printf("---Smarca: Sto scrivendo 1 in un nodo non-foglia\n");
			printf("Sono al livello %d, indice container(%u)\n", level(parent_fix),parent_fix->container_pos);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", new_val, old_val);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", BITMASK_LEAF(new_val, parent_fix->container_pos), BITMASK_LEAF(old_val, parent_fix->container_pos));
			abort();
		}
		if(BITMASK_LEAF(new_val, parent_fix->container_pos) & 0x10ULL){
			printf("Smarca: Sto scrivendo 1 in un nodo non-foglia\n");
			printf("Sono al livello %d, indice container(%u)\n", level(parent_fix),parent_fix->container_pos);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", new_val, old_val);
			printf("scrivendo il valore %llx al posto di %llx in un nodo non-foglia\n", BITMASK_LEAF(new_val, parent_fix->container_pos), BITMASK_LEAF(old_val, parent_fix->container_pos));
			abort();
		}
#endif	
		
	}while(!__sync_bool_compare_and_swap(&(parent(n).container->nodes), old_val, new_val));
	
	if(current != upper_bound && !exit) //OCCHIO perchè current è la radice successiva
		smarca_(current); //devo andare su current che sarebbe la radice successiva;
	
}


/*
void find_the_bug_on_new_val(unsigned long long new_val){
	unsigned long long val[15];
	val[0] = (new_val & ((unsigned long long) (0x1)));
	val[1] = (new_val & ((unsigned long long) (0x1<<1))) >> 1;
	val[2] = (new_val & ((unsigned long long) (0x1<<2))) >> 2;
	val[3] = (new_val & ((unsigned long long) (0x1<<3))) >> 3;
	val[4] = (new_val & ((unsigned long long) (0x1<<4))) >> 4;
	val[5] = (new_val & ((unsigned long long) (0x1<<5)))>> 5;
	val[6] = (new_val & ((unsigned long long) (0x1<<6)))>> 6;
	val[7] = (new_val & (LEAF_FULL_MASK << ((7) + (5 * ((8-1) - (7)))))) >> ((7) + (5 * ((8-1) - (7))));
	val[8] = (new_val & (LEAF_FULL_MASK << ((7) + (5 * ((9-1) - (7)))))) >> ((7) + (5 * ((9-1) - (7))));
	val[9] = (new_val & (LEAF_FULL_MASK << ((7) + (5 * ((10-1) - (7)))))) >> ((7) + (5 * ((10-1) - (7))));
	val[10] = (new_val & (LEAF_FULL_MASK << ((7) + (5 * ((11-1) - (7)))))) >> ((7) + (5 * ((11-1) - (7))));
	val[11] = (new_val & (LEAF_FULL_MASK << ((7) + (5 * ((12-1) - (7)))))) >> ((7) + (5 * ((12-1) - (7))));
	val[12] = (new_val & (LEAF_FULL_MASK << ((7) + (5 * ((13-1) - (7)))))) >> ((7) + (5 * ((13-1) - (7))));
	val[13] = (new_val & (LEAF_FULL_MASK << ((7) + (5 * ((14-1) - (7)))))) >> ((7) + (5 * ((14-1) - (7))));
	val[14] = (new_val & (LEAF_FULL_MASK << ((7) + (5 * ((15-1) - (7)))))) >> ((7) + (5 * ((15-1) - (7))));
	int i;
	for(i=0;i<15;i++){
		if(val[i]!=1 && val[i]!=2 && val[i]!=10 && val[i]!=5 && val[i]!=19 && val[i]!=0 && val[i]!=15 && val[i]!=3 && val[i]!=11 && val[i]!=7) {
			printf("\n è uscito: %llu!!!!!\n", val[i]);
			abort();
		}
	}
}
*/
/*
void  find_the_bug(int who){
	int i;
	
	for(i=1;i<=number_of_nodes;i++){
		node* n = &tree[i];
		unsigned long long val = VAL_OF_NODE(n);
		if(val!=1 && val!=2 && val!=10 && val!=5 && val!=19 && val!=0 && val!=15 && val!=3 && val!=11 && val!=7){
			if(who==0)
				printf("è stata la alloc e ha detto %llu\n", val);
			else if(who==1)
				printf("è stata la free e ha detto %llu\n", val);
			else if(who==2)
				printf("è stata proprio la free e ha detto %llu\n", val);
			else if(who==3)
				printf ("è stata la smarca e ha detto %llu\n", val);
			else if(who==4)
				printf("è stata la marca e ha detto %llu\n", val);
			else if(who==5)
				printf("è stata la free_node con 5 e ha detto %llu\n", val);
			abort();
		}
	}
	
}
*/


//MARK: WRITE SU FILE

/* 
	SCRIVE SU FILE LA SITUAZIONE DELL'ALBERO (IN AMPIEZZA) VISTA DA UN CERTO THREAD
 */

void write_on_a_file_in_ampiezza(){
	char filename[128];
	int i;
#ifdef NUMA
	int actual_node;
	for(actual_node=0;actual_node<number_of_numa_nodes;actual_node++){
		tree = overall_trees[actual_node];
		//puts("stampo");
		sprintf(filename, "./debug/tree%d_%d.txt", actual_node, getpid());
		FILE *f = fopen(filename, "w");
		
		if (f == NULL){
			printf("Error opening file!\n");
			exit(1);
		}
		fprintf(f, "%d \n", getpid());
		
		for(i=1;i<=number_of_nodes;i++){
			node* n = &tree[i];
			fprintf(f, "(%p) %u val=%llu has %llu B. mem_start in %llu  level is %u\n", (void*)n, tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));
		}
		
		fclose(f);
	}
	
#else
	sprintf(filename, "./debug/tree%d.txt", getpid() - master);
	FILE *f = fopen(filename, "w");
	
	if (f == NULL){
		printf("Error opening file!\n");
		exit(1);
	}
	//fprintf(f, "%d \n", getpid());
	
	for(i=1;i<=number_of_nodes;i++){
		node* n = &tree[i];
		fprintf(f, "(%p) %u\t val=%llu has %llu B. mem_start in %llu  level is %u\n", (void*)n, tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));
	}
	
	fclose(f);
#endif
}

void write_on_a_file_in_ampiezza_start(){
	char filename[128];
	int i;
#ifdef NUMA
	int actual_node;
	for(actual_node=0;actual_node<number_of_numa_nodes;actual_node++){
		tree = overall_trees[actual_node];
		printf("shampo\n");
		sprintf(filename, "./debug/init_tree%d_%d.txt", actual_node, getpid());
		FILE *f = fopen(filename, "w");
		
		if (f == NULL){
			printf("Error opening file!\n");
			exit(1);
		}
		fprintf(f, "%d \n", getpid());
		
		for(i=1;i<=number_of_nodes;i++){
			node* n = &tree[i];
			fprintf(f, "(%p) %u val=%llu has %llu B. mem_start in %llu  level is %u\n", (void*)n, tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));
		}
		
		fclose(f);
	}
	
#else
	sprintf(filename, "./debug/init_tree%d.txt", getpid() - master);
	FILE *f = fopen(filename, "w");
	
	if (f == NULL){
		printf("Error opening file!\n");
		exit(1);
	}
	//fprintf(f, "%d \n", getpid());
	
	for(i=1;i<=number_of_nodes;i++){
		node* n = &tree[i];
		fprintf(f, "(%p) %u val=%llu has %llu B. mem_start in %llu  level is %u\n", (void*)n, tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));
	}
	
	fclose(f);
#endif
}
/*
void write_on_a_file_touched(){
	char filename[128];
	sprintf(filename, "./debug/touched.txt");
	FILE *f = fopen(filename, "w");
	int i;
	
	if (f == NULL){
		printf("Error opening file!\n");
		exit(1);
	}
	
	for(i=1;i<=number_of_nodes;i++){
		node* n = &tree[i];
		if(VAL_OF_NODE(n)!=0){
			fprintf(f,"%u\n", n->pos);
		}
	}
	
	fclose(f);
}
*/

//MARK: PRINT

/*traversal tramite left and right*/
/*
void print_in_profondita(node* n){
	printf("%u has\n", n->pos);
	printf("%u has %llu B. mem_start in %llu left is %u right is %u status=%llu level=%u\n", n->pos, n->mem_size, n->mem_start, left_index(n), right_index(n), VAL_OF_NODE(n), level(n));
	if(left_index(n)<= number_of_nodes){
		print_in_profondita(&left(n));
		print_in_profondita(&right(n));
	}
}
*/
/*Print in ampiezza*/
/*
void print_in_ampiezza(){
	int i;
	for(i=1;i<=number_of_nodes;i++){
		node* n = &tree[i];
		//per il momento stampiano la stringa
		printf("%u val=%llu has %llu B. mem_start in %llu  level is %u\n", tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));	  
	}
}
*/

unsigned int count_occupied_leaves(){
	unsigned int starting_node, count = 0;
	node * n;
 
	starting_node = number_of_leaves;
	
	for(int i=0; i<number_of_leaves; i++){
		n = &(tree[starting_node+i]);
		if(!IS_ALLOCABLE(n->container->nodes, n->container_pos))
			count++;
	}	
	return count;
}
