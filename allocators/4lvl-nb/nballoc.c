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

#define LEAF_FULL_MASK		(0x1FULL)
#define LEFT  				(0x2ULL)
#define RIGHT				(0x1ULL)
#define COAL_LEFT			(0x8ULL)
#define COAL_RIGHT			(0x4ULL)
#define LOCK_LEAF			(0x13ULL)
#define TOTAL 				(0xffffffffffffffffULL)
#define LOCK_LEAF_MASK		(0x13ULL)
#define LOCK_NOT_LEAF_MASK	(0x1ULL)
#define LEAF_MASK_SIZE 		(0x5ULL)

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

#define CHECK_BROTHER_OCCUPIED(n_pos, actual_value) \
		{ \
			if(\
			((is_left_by_idx(n_pos)) && (!IS_ALLOCABLE(actual_value, (n_pos+1)))) || \
			((is_right_by_idx(n_pos)) && (!IS_ALLOCABLE(actual_value,(n_pos-1))))\
			){ do_exit=true; break;}\
		}

#define VAL_OF_NODE(n) ((unsigned long long) (n->container_pos<LEAF_START_POSITION ) ? ((n->container->nodes & (0x1ULL << (n->container_pos-1))) >> (n->container_pos-1)) : ((n->container->nodes & (LEAF_FULL_MASK << ((LEAF_START_POSITION-1) + (5 * ((n->container_pos-1) - (LEAF_START_POSITION-1))))))) >> ((LEAF_START_POSITION-1) + (5 * ((n->container_pos-1) - (LEAF_START_POSITION-1)))))

#define ROOT 			(tree[1])

#define level(n) ((unsigned int) ( (overall_height) - (log2_(( (n)->mem_size) / (MIN_ALLOCABLE_BYTES )) )))
#define level_by_idx(n) ( 1 + (log2_(n)))

#define lchild_idx_by_ptr(n)   (((n)->pos)*2)
#define rchild_idx_by_ptr(n)   (lchild_idx_by_ptr(n)+1)
#define parent_idx_by_ptr(n)   (((n)->pos)/2)

#define lchild_idx_by_idx(n)   (n << 1)
#define rchild_idx_by_idx(n)   (lchild_idx_by_idx(n)+1)
#define parent_idx_by_idx(n)   (n >> 1)

#define lchild_ptr_by_ptr(n)   (tree[lchild_idx_by_ptr(n)])
#define rchild_ptr_by_ptr(n)   (tree[rchild_idx_by_ptr(n)])
#define parent_ptr_by_ptr(n)   (tree[parent_idx_by_ptr(n)])

#define lchild_ptr_by_idx(n)   (tree[lchild_idx_by_idx(n)])
#define rchild_ptr_by_idx(n)   (tree[rchild_idx_by_idx(n)])
#define parent_ptr_by_idx(n)   (tree[parent_idx_by_idx(n)])

#define is_leaf_by_idx(n) ((n) >= (LEAF_START_POSITION)) //attenzione: questo ti dice se il figlio è tra le posizione 8-15. Se sei foglia di un grappolo piccolo qua non lo vedi
#define is_left_by_idx(n)	(1ULL & (~(n)))
#define is_right_by_idx(n)	(1ULL & ( (n)))

//PARAMETRIZZAZIONE
#define LEVEL_PER_CONTAINER 4

/* VARIABILI GLOBALI *//*---------------------------------------------------------------------------------------------*/

node* tree; //array che rappresenta l'albero, tree[0] è dummy! l'albero inizia a tree[1].
node_container* containers; //array di container In Numa questo sarà uno specifico tree (di base tree[getMyNUMANode])
unsigned long long overall_memory_size;
unsigned long long overall_memory_pages;
unsigned long long overall_height;
unsigned int number_of_leaves;
unsigned int number_of_nodes; //questo non tiene presente che tree[0] è dummy! se qua c'è scritto 7 vuol dire che ci sono 7 nodi UTILIZZABILI
unsigned int number_of_processes;
unsigned int number_of_container;
static volatile unsigned long long levels = NUM_LEVELS;

#ifdef NUMA
	unsigned int number_of_numa_nodes;
	bool allocate_in_remote_node = true; //make it possible to allocate in remote no. set to false if you don't want.
	node** overall_trees; //array di alberi! Un albero per ognu NUMA node
	node_container** overall_containers; //Uno per ogni albero (uno per ogni nodo)
	void** overall_memory; //array of pointers. One pointer for each numa NODE
#else
	void* overall_memory;
#endif

//node* global_upper_bound;//TODO

#ifdef DEBUG
unsigned long long *node_allocated, *size_allocated;
#endif


//node* BUNCHROOT(node* n) {
//	if(!IS_BUNCHROOT(n->container->bunch_root)) 
//		printf("NO BUNCH ROOT\n"); 
//	return n->container->bunch_root;
//}
//
//int IS_BUNCHROOT(node *n){
//	return n->container_pos == (1); 
//} 



/* DICHIARAZIONE DI FUNZIONI *//*---------------------------------------------------------------------------------------------*/

static void init_tree(unsigned long long number_of_nodes);
//void init();
void end();
static unsigned long long alloc(node* n);
static unsigned long long alloc_mauro(node* n);
static void marca(node* n, node* upper_bound);
static void marca_mauro(node* n, node* upper_bound);
static bool IS_OCCUPIED(unsigned long long, unsigned);
static unsigned long long check_parent(node* n);
static unsigned long long check_parent_mauro(node* n);
static void smarca(node* n, node* upper_bound);
static void smarca_mauro(node* n, node* upper_bound);
static void internal_free_node(node* n, node* upper_bound);
static void internal_free_node_mauro(node* n, node* upper_bound);
void* request_memory(unsigned int pages);
#ifdef NUMA
void do_move(void * buffer, unsigned int target_node, unsigned int n_pages);
#endif


static node* volatile free_tree = NULL; //array che rappresenta l'albero, tree[0] è dummy! l'albero inizia a tree[1]




/* FUNZIONI *//*---------------------------------------------------------------------------------------------*/

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
	j++;
	tree = overall_trees[j];
	containers = overall_containers[j];
#endif

	number_of_container = 0;
	
	ROOT.mem_start = 0ULL;
	ROOT.mem_size = overall_memory_size;
	ROOT.pos = 1ULL;
	ROOT.container = &containers[number_of_container++];
	ROOT.container_pos = 1ULL;
	ROOT.container->bunch_root = &ROOT;
    ROOT.container->nodes = 0ULL;
#ifdef NUMA
	ROOT.numa_node = j;
#endif
	
	//init_node(ROOT);
	for(i=2;i<=number_of_nodes;i++){
		tree[i].pos = i;
		node parent = parent_ptr_by_ptr(&tree[i]);
		tree[i].mem_size = (parent.mem_size) / 2;
		tree[i].mem_start = (parent.mem_start) + ((tree[i].mem_size)&(lchild_idx_by_ptr(&parent)==i));
		
#ifdef NUMA
		tree[i].numa_node = j;
#endif
		
		if(level(&tree[i])%LEVEL_PER_CONTAINER==1){
			tree[i].container = &containers[number_of_container++];
			tree[i].container_pos = 1ULL;
			tree[i].container->nodes = 0ULL;
			tree[i].container->bunch_root = &tree[i];
		}
		else{
			tree[i].container = parent.container;
			tree[i].container_pos = (parent.container_pos*2)+(1&(lchild_idx_by_ptr(&parent)!=i));
		}
	}

#ifdef NUMA
	if(j != number_of_numa_nodes-1)
		goto next_node_label;
#endif
}



#ifdef NUMA
/*
	@param levels:  Numero di livelli dell'albero - NUMA version
 */
void init(){
	
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
void init_BANANA(){
	number_of_nodes = (1<<levels) - 1;
	
	number_of_leaves = (1<< (levels-1));
	overall_memory_size = MIN_ALLOCABLE_BYTES * number_of_leaves;
	overall_height = levels;

	overall_memory 	= mmap(NULL, overall_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	tree 			= mmap(NULL,(1+number_of_nodes)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	containers 		= mmap(NULL,(number_of_nodes-1)*sizeof(node_container), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	free_tree  		= mmap(NULL,64+(number_of_leaves)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
     
	if(overall_memory==MAP_FAILED || tree==MAP_FAILED || containers==MAP_FAILED || free_tree==MAP_FAILED){
		puts("Failing allocating structures\n");
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
	printf("\t Levels = %10llu\n", overall_height);
	printf("\t Leaves = %10u\n", (number_of_nodes+1)/2);
	printf("\t Nodes  = %10u\n", number_of_nodes);
	printf("\t Containers = %u\n", number_of_container);
	printf("\t Min size %12llu at level %2llu\n", MIN_ALLOCABLE_BYTES, overall_height);
	printf("\t Max size %12llu at level %2llu\n", MAX_ALLOCABLE_BYTE, overall_height - log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES));
	
}
#endif

__attribute__ ((constructor)) void  premain(){
    init_BANANA();
	//printf("UMA Init complete\n");
}

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
	free(tree);
#endif
}


//MARK: ALLOCAZIONE


/*
 Questa funziona controlla se il nodo in posizione container_pos è TOTALMENTE occupato (se si tratta di una foglia) oppure se è parzialmente o totalmente occupato se non è una foglia, nella maschera dei nodi rappresentata da val. Ricordo che per la semantica dell'algoritmo una foglia è totalmente occupata se e solo ha il quinto bit settato.
 @param val: il container nel quale il nodo si trova
 @param pos: il container_pos del nodo che vogliamo controllare
 
 */
static inline bool IS_OCCUPIED(unsigned long long val, unsigned int pos){
	if(pos==0)
	{
		printf("AAAAAAAAAAAAH\n");
		exit(1);
	}
	if(pos < LEAF_START_POSITION)
		return ( (val & ((LOCK_NOT_LEAF_MASK) << (pos - 1))) != 0);
	else//è una foglia
		return val & (( (LOCK_NOT_LEAF_MASK) << (LEAF_START_POSITION - 2)) << (5* (pos - (LEAF_START_POSITION - 1))));
}


/*
 Funzione di malloc richiesta dall'utente.
 @param pages: memoria richiesta dall'utente
 @return l'indirizzo di memoria del nodo utilizzato per soddisfare la richiesta; NULL in caso di fallimento
 */
void* request_memory(unsigned int byte){
	bool restarted = false; 
	unsigned long long started_at, actual, starting_node, last_node, failed_at, leaf_position;
	
	if(byte > MAX_ALLOCABLE_BYTE)
		return NULL;
		
	byte = upper_power_of_two(byte);
	
	if(byte < MIN_ALLOCABLE_BYTES)
		byte = MIN_ALLOCABLE_BYTES;
	
	starting_node = overall_memory_size / byte; //first node for this level
	last_node = lchild_idx_by_idx(starting_node)-1;//left_index(&tree[starting_node])-1; //last node for this level
		
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
   
	//quando faccio un giro intero ritorno NULL
	do{
		if((failed_at = alloc(&tree[actual])) == 0){ //TODO: fai tornare a alloc il failed_node
#ifdef DEBUG
			__sync_fetch_and_add(node_allocated,1);
			__sync_fetch_and_add(size_allocated,byte);
#endif

            leaf_position = byte*(actual - overall_memory_size / byte)/MIN_ALLOCABLE_BYTES;
            free_tree[leaf_position].pos = actual;
			return ((char*) overall_memory)+leaf_position*MIN_ALLOCABLE_BYTES;
		}
#ifdef NUMA
		if(failed_at==1){ // il buddy è pieno
			if(allocate_in_remote_node && ((trying_on_numa_node+1)%number_of_numa_nodes) != starting_numa_node){
				trying_on_numa_node = (trying_on_numa_node+1)%number_of_numa_nodes;
				goto try_on_a_new_numa_node_label;
			}
			else
				return NULL;
		}
#else
		if(failed_at==1){ // il buddy è pieno //TODO: secondo me questo non serve
			return NULL;
		}
#endif
		
		//Questo serve per evitare tutto il sottoalbero in cui ho fallito
		actual = (failed_at + 1) * (1 << ( level_by_idx(actual) - level_by_idx(failed_at) ) );
		
		if(actual > last_node){ //se ho sforato riparto dal primo utile, se il primo era quello da cui avevo iniziato esco al controllo del while
			actual = starting_node;
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
static inline unsigned long long occupa_container(unsigned long long n_pos, unsigned long long new_val){//TODO
	unsigned long long i=0, tmp_val, /*str_lvl,*/ starting, ending, curr_pos;
	
	//if(left_index(n) >= number_of_nodes)//MAURO: non sono sicuro sia corretto... //TODO
	//	return new_val;						//ma infondo, serve?
	
	//Scrivo i nodi da n (escluso) fino alla radice
	curr_pos = n_pos;
	while((curr_pos/=2) != 0){
		new_val = LOCK_NOT_A_LEAF(new_val, curr_pos); 
	}
	
	//lvl = level_from_root_by_idx(n_pos); //[1,4] = numero del livello
	curr_pos = n_pos;
	tmp_val = 1ULL;
	
	//Scrivo i nodi da n (incluso) fino alle foglie
	while(curr_pos < LEAF_START_POSITION){
		i++;
		new_val |= (tmp_val << (curr_pos-1));
		tmp_val |= (tmp_val << (curr_pos/n_pos));
		curr_pos *= 2;
	}
	
	starting = (n_pos << i);
	ending = starting + (1<<i);
	for(i = starting ; i < ending ; i++){
		new_val = LOCK_A_LEAF(new_val, i);
	}
	return new_val;
}


/*
	Questa funziona controlla se il nodo in posizione container_pos è libero nella maschera dei nodi rappresentata da val. Per la semantica dell'algoritmo un nodo è allocabile se e solo se esso è completamente libero, i.e. il nodo è completamento 0. Per questo motivo per le foglie il check è fatto con 0x1F
	@param val: il container nel quale il nodo si trova
	@param pos: il container_pos del nodo che vogliamo controllare
 
 */
static inline bool IS_ALLOCABLE(unsigned long long val, unsigned int pos){
	if(pos==0)
	{
		printf("AAAAAAAAAAAAH\n");
		exit(1);
	}
	if(pos < LEAF_START_POSITION) //non è una foglia
		return !( (val & ((LOCK_NOT_LEAF_MASK) << (pos-1))) != 0);
	else
		return !(val & (( (LEAF_FULL_MASK) << (LEAF_START_POSITION-1)) << (5* (pos-(LEAF_START_POSITION)))));
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
unsigned long long alloc(node* n){ // non conviene andare con l'index perchè tanto serve il puntatore al container
	unsigned long long old_val, new_val, n_pos/*, curr_pos*/, failed_at;
	
	n_pos = n->container_pos;
	
	do{
		new_val = old_val = n->container->nodes;
		
		if(!IS_ALLOCABLE(new_val, n_pos)){
			return n->pos;
			//failed_at_node = n->pos;
			//return false;
		}
		
		new_val = occupa_container(n_pos, new_val);
		
	}while(new_val!=old_val && !__sync_bool_compare_and_swap(&n->container->nodes, old_val, new_val));
	//Se n appartiene al grappolo della radice
	
	if(n->container->bunch_root == &ROOT){
		return 0;//true;
	}

	return check_parent(n);//false;

}


/*
 Questa funzione si preoccupa di marcare il bit relativo al sottoalbero del blocco che ho allocato in tutti i suoi antenati.
 Questa funziona ritorna true se e solo se riesce a marcare fino alla radice. La funzione fallisce se e solo se si imbatte in un nodo occupato (ricordando che se un generico nodo è occupato, allora la foglia ad esso relativa (il nodo stesso o un suo discendente) avrà il falore 0x13)
 La funzione si preoccupa inoltre di cancellare l'eventuale bit di coalescing.
 Side effect: se fallisce la variabile globale failed_at_node assume il valore del nodo dove la ricorsione è fallita (TODO: occhio, fallisco sulla foglia, magari il nodo bloccato era il padre, non ho modo di saperlo, rischio di sbattare nuovamente su questo nodo)
 @param n: nodo da cui iniziare la risalita (che è già marcato totalmente o parzialmente). Per costruzione della alloc, n è per forza un nodo radice di un grappolo generico (ma sicuramente non la radice)
 @return true se la funzione riesce a marcare tutti i nodi fino alla radice; false altrimenti.
 
 */
static unsigned long long check_parent(node* n){
	unsigned long long new_val, old_val, tmp_container_pos, n_pos, p_pos;
	node* parent = n, *upper_bound, *bunchroot;
	do{
		bunchroot = BUNCHROOT(parent);
		parent = &parent_ptr_by_ptr(bunchroot);
		upper_bound = bunchroot;
		p_pos = parent->container_pos; 
		
		do{
			tmp_container_pos = p_pos; 
			new_val = old_val = parent->container->nodes;
			
			if(IS_OCCUPIED(parent->container->nodes, tmp_container_pos)){
				internal_free_node(n, upper_bound);
				return parent->pos;
			}
			
			//CON QUESTA PEGGIORA? ERA SBAGLIATA
			//new_val = (  	(-( is_left_by_idx(n->pos)))&( OCCUPY_LEFT( CLEAN_LEFT_COALESCE(new_val, parent->container_pos), parent->container_pos)) |
			//				(-(!is_left_by_idx(n->pos)))&(OCCUPY_RIGHT(CLEAN_RIGHT_COALESCE(new_val, parent->container_pos), parent->container_pos)) );
			
			if(is_left_by_idx(bunchroot->pos)/*&left(parent)==n*/){
				new_val = CLEAN_LEFT_COALESCE(new_val, tmp_container_pos);
				new_val = OCCUPY_LEFT(new_val, tmp_container_pos);
			}else{
				new_val = CLEAN_RIGHT_COALESCE(new_val, tmp_container_pos);
				new_val = OCCUPY_RIGHT(new_val, tmp_container_pos);
			}
			
			while((tmp_container_pos/=2) != 0){
				new_val = LOCK_NOT_A_LEAF(new_val, tmp_container_pos);
			}
			
		}while(new_val!=old_val && !__sync_bool_compare_and_swap(&(parent->container->nodes),old_val,new_val));
	}while(BUNCHROOT(parent) != &ROOT);	
	
	return 0;//true;
}

//MARK: FREE

/*
 Duale della occupa_discendenti
 Questa è una funzione di help per la free. Libera tutti i discendenti del nodo n presenti nello stesso grappolo. NB questa funzione modifica solo new_val; non fa CAS, la modifica deve essere apportata dal chiamante
 @param n: il nodo a cui liberare i discendenti
 @param new_val: sarebbe  n->container->nodes da modificare
 
 */
//static unsigned long long libera_discendenti(node* n, unsigned long long new_val){ //TODO
//	//se l'ultimo grappolo non ha tutti i nodi che deve avere (tipo ha solo due livelli). QUesto quando andiamo a regime non dovrebbe succedere perchè questa situazione la evitiamo (inutile avere un grappolo monco.. il numero di cas è lo stesso
//	if(left_index(n)>=number_of_nodes)
//		return new_val;
//	
//	if(!IS_LEAF(&lchild_ptr_by_ptr(n))){
//		new_val = UNLOCK_NOT_A_LEAF(new_val, lchild_ptr_by_ptr(n).container_pos);
//		new_val = UNLOCK_NOT_A_LEAF(new_val, rchild_ptr_by_ptr(n).container_pos);
//		new_val = libera_discendenti(&lchild_ptr_by_ptr(n), new_val);
//		new_val = libera_discendenti(&rchild_ptr_by_ptr(n), new_val);
//	}
//	else{
//		new_val = UNLOCK_A_LEAF(new_val,lchild_ptr_by_ptr(n).container_pos);
//		new_val = UNLOCK_A_LEAF(new_val,rchild_ptr_by_ptr(n).container_pos);
//	}
//	return new_val;
//}

static inline unsigned long long libera_container(unsigned long long n_pos, unsigned long long new_val, bool* do_exit2){//TODO
	unsigned long long i=0, tmp_val, /*str_lvl,*/ starting, ending, curr_pos;
	bool do_exit = false;
	
	//if(left_index(n) >= number_of_nodes)//MAURO: non sono sicuro sia corretto... //TODO
	//	return new_val;						//ma infondo, serve?

	curr_pos = n_pos;
	while(curr_pos != 1){
		do_exit = false;
			CHECK_BROTHER_OCCUPIED(curr_pos,new_val); //questo termina il ciclo se il fratello è occupato e setta exit = true
			curr_pos/=2;
			new_val = UNLOCK_NOT_A_LEAF(new_val, curr_pos);
	}
	*do_exit2 = do_exit; 

	
	//lvl = level_from_root_by_idx(n_pos); //[1,4] = numero del livello
	curr_pos = n_pos;
	tmp_val = 1ULL;
	
	while(curr_pos < LEAF_START_POSITION){
		new_val &= ~(tmp_val << (curr_pos-1));
		tmp_val |=  (tmp_val << (curr_pos/n_pos));
		curr_pos *= 2;
		i++;
	}
	
	starting = (n_pos<<i);
	ending = starting + (1<<i);
	for(i = starting ; i < ending ; i++){
		new_val = UNLOCK_A_LEAF(new_val, i);
	}
	return new_val;
}


void free_node(void* n){
#ifdef NUMA
	tree = overall_trees[n->numa_node];
	containers = overall_containers[n->numa_node];
#endif

    char * tmp = ((char*)n) - (char*)overall_memory;
    unsigned long long pos = (unsigned long long) tmp;
    pos = pos / MIN_ALLOCABLE_BYTES;

	internal_free_node(&tree[free_tree[pos].pos], &ROOT);

#ifdef DEBUG
	__sync_fetch_and_add(node_allocated,-1);
	__sync_fetch_and_add(size_allocated,-(n->mem_size));
#endif
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
static void internal_free_node(node* n, node* upper_bound){
	unsigned long long old_val, new_val, p_pos, n_pos;
	bool do_exit = false;
	//printf("LIBERO TAGLIA %d\n", n->mem_size);
	
#ifdef NUMA
	tree = overall_trees[n->numa_node];
	containers = overall_containers[n->numa_node];
#endif
	
	if(BUNCHROOT(n) != upper_bound)//TODO: sistemare marca per togliere il controllo qui
		marca(BUNCHROOT(n), upper_bound);
	//LIBERA TUTTO CIO CHE RIGUARDA IL NODO E I SUOI DISCENDENTI ALL INTERNO DEL GRAPPOLO. ATTENZIONE AI GENITORI ALL'INTERNO DEL GRAPPOLO (p.es. se il fratello è marcato; non smarcare il padre). Nota che fino a smarca non mi interessa di upper_bound
	do{
		n_pos = p_pos = n->container_pos; 
		old_val = new_val = n->container->nodes;
		new_val = libera_container(n_pos, new_val, &do_exit);
	}while(new_val!=old_val && !__sync_bool_compare_and_swap(&n->container->nodes,old_val, new_val));
	
	if(BUNCHROOT(n) != upper_bound && !do_exit)
		smarca(BUNCHROOT(n), upper_bound);
}


unsigned long long alloc_mauro(node* n){ // non conviene andare con l'index perchè tanto serve il puntatore al container
	unsigned long long old_val, new_val, n_pos, /*curr_pos,*/ failed_at;
	
	n_pos = n->container_pos;
	
	do{
		new_val = old_val = n->container->nodes;
		
		if(!IS_ALLOCABLE(new_val, n_pos)){
			return n->pos;
			//failed_at_node = n->pos;
			//return false;
		}
		
		new_val = occupa_container(n_pos, new_val);
		
		
	//	//marco i genitori in questo grappolo Sicuramente non è una foglia visto che parto da un genitore
	//	curr_pos = n_pos;
	//	while((curr_pos/=2) != 0){
	//		new_val = LOCK_NOT_A_LEAF(new_val, curr_pos); 
	//	}
	//	
	//	//marco n se è una foglia
	//	if(is_leaf_by_idx(n_pos)){
	//		new_val = LOCK_A_LEAF(new_val, n_pos);
	//	}
	//	
	//	//marco n ed i suoi figli se n non è una foglia
	//	else{
	//		//marco n
	//		new_val = LOCK_NOT_A_LEAF(new_val,n_pos); //TODO: accorpa nella riga sotto
	//		new_val = occupa_discendenti(n, n_pos, new_val);
	//	}
		
	}while(new_val!=old_val && !__sync_bool_compare_and_swap(&n->container->nodes, old_val, new_val));
	//Se n appartiene al grappolo della radice
	
	if(n->container->bunch_root == &ROOT){
		return 0;//true;
	}
	return check_parent(n);//return check_parent(BUNCHROOT(n));
	//else if( (failed_at = check_parent(BUNCHROOT(n))) == 0 ){
	//	return 0;//true;
	//}
	//else{
	//	free_node_(n);//TODO: mi insospettisce, come capisce dove fermarsi?
	//	return failed_at;//false;
	//}
}



void marca_mauro(node* n, node* upper_bound){
	node* parent = n;
	unsigned long long old_val, new_val, p_pos;
	
	do{
		parent = &parent_ptr_by_ptr(BUNCHROOT(parent));
		p_pos = parent->container_pos;
		
		do{
			old_val = new_val = parent->container->nodes;
			new_val = new_val | (COALESCE_RIGHT(0, p_pos) << is_left_by_idx(p_pos));
			//if (is_left_by_idx(p_pos)) new_val = COALESCE_LEFT(old_val, p_pos);
			//else new_val = COALESCE_RIGHT(old_val, p_pos);
		}while(new_val!=old_val && !__sync_bool_compare_and_swap(&parent->container->nodes, old_val, new_val));
	}while(BUNCHROOT(parent) != upper_bound);
}




/*
 Funzione ausiliaria a free_node_ (in particolare al primo step della free). In questa fase la free vuole marcare tutti i nodi dei sottoalberi interessati dalla free come "coalescing". Qui n è la radice di un grappolo. Bisogna mettere il bit di coalescing al padre (che quindi sarà una foglia con 5 bit). Nota che questa funzione non ha ragione di esistere se il nodo da liberare è nel grappolo di upper_bound.
 @param n è la radice di un grappolo. Bisogna settare in coalescing il padre.
 @return il valore precedente con un singolo nodo marcato come "coalescing"
 */
static void marca(node* n, node* upper_bound){
	node* parent = n, *tmp_parent;// &parent(n);
	unsigned long long old_val, new_val, p_pos, c_pos;
	

	do{
		tmp_parent = parent;

		bool banana = false;
		c_pos = BUNCHROOT(parent)->pos;
		parent = &parent_ptr_by_ptr(BUNCHROOT(parent));
		
		if(0)
			if(parent->pos == 9 || parent->pos == 18)
			{
				printf("MARCA INIZIO %d\n", VAL_OF_NODE(parent));
				banana = true;
			}
		
		p_pos = parent->container_pos;
		do{
			new_val = old_val = parent->container->nodes;
			
			if(0)
			if(banana)
			{
				 parent->container->nodes = new_val;
				 printf("BANANA  PA %d\n", VAL_OF_NODE(parent) );
				 parent->container->nodes = old_val;
				}

			
			//new_val = ((-1+is_left_by_idx(p_pos)) & COALESCE_RIGHT(old_val, p_pos)) | ((-is_left_by_idx(p_pos)) & COALESCE_LEFT(old_val, p_pos))  ;
			if (is_left_by_idx(c_pos) == 1)
				new_val = COALESCE_LEFT(new_val, p_pos);
			else
				new_val = COALESCE_RIGHT(new_val, p_pos);
			if(0)
			if(banana)
			{
				 parent->container->nodes = new_val;
				 printf("BANANA  PA %d\n", VAL_OF_NODE(parent));
				 parent->container->nodes = old_val;
				}
		}while(new_val!=old_val && !__sync_bool_compare_and_swap(&parent->container->nodes, old_val, new_val));
		
		//	parent = &parent_ptr_by_ptr(BUNCHROOT(parent));//parent = &parent(parent); //QUESTO HA RIDOTTO DEL 20 PER CENTO I TEMPI.. WTF!!!! (TODO: solo per richiamare attenzione)
		//	p_pos = parent->container_pos;
	}while(BUNCHROOT(parent) != upper_bound);
	//find_the_bug(4);
	//if(BUNCHROOT(parent)!=upper_bound)
	//	marca(BUNCHROOT(parent));
}



/*
 Questa funziona leva il bit di coalesce e libera il sottoramo (fino all'upper bound) ove necessario. Nota che potrebbe terminare lasciando alcuni bit di coalescing posti ad 1. Per i dettagli si guardi la documentazione. Upper bound è l'ultimo nodo da smarcare. n è la radice di un grappolo. Va pulito il coalescing bit al padre di n (che è su un altro grappolo). Quando inizio devo essere certo che parent(n) abbia il coalescing bit settato ad 1.
		La funzione termina prima se il genitore del nodo che voglio liberare ha l'altro nodo occupato (sarebbe un errore liberarlo)
 @param n: n è la radice di un grappolo (BISOGNA SMARCARE DAL PADRE)
 
 */
static inline void smarca(node* n, node* upper_bound){
	unsigned long long old_val, new_val, p_pos, c_pos;
	bool do_exit=false;
	node /**current,*/ *parent;
	parent = n;
	bool banana;

	do{

		c_pos = BUNCHROOT(parent)->pos;
		parent = &parent_ptr_by_ptr(BUNCHROOT(parent));//&parent(parent); //
		p_pos = parent->container_pos;
			
		do{
			banana = false;
			do_exit = false;
			
			//TODO: questo if_else può essere semplificato di molto con le giuste macro...probabilmente non influisce sulle performance
			old_val = new_val = parent->container->nodes;

			if(is_left_by_idx(c_pos)){
				
				if(!IS_COALESCING_LEFT(new_val, p_pos)) //qualcuno l'ha già pulito
					return;
				
				new_val = CLEAN_LEFT_COALESCE(new_val, p_pos); //lo facciamo noi
				new_val = CLEAN_LEFT(new_val, p_pos);

				//SE il fratello è occupato vai alla CAS
				if(IS_OCCUPIED_RIGHT(new_val, p_pos))
				{
					do_exit = true;
					continue;
				}

			}
			else{
				if(!IS_COALESCING_RIGHT(new_val, p_pos)) //qualcuno l'ha già pulito
					return;

				new_val = CLEAN_RIGHT_COALESCE(new_val, p_pos); //lo facciamo noi
				new_val = CLEAN_RIGHT(new_val, p_pos);
				
				//SE il fratello è occupato vai alla CAS
				if(IS_OCCUPIED_LEFT(new_val, p_pos)){
					do_exit = true;
					continue;
				}
			}
			
			//ORA VADO A SMARCARE TUTTI GLI ALTRI NODI NON FOGLIA DI QUESTO GRAPPOLO
			//current = parent;
			//parent = &parent(current);
			//while(current!=BUNCHROOT(current)){
			//	//questo termina il ciclo se il fratello è occupato e setta exit = true
			//	CHECK_BROTHER(parent, current, new_val);
			//	
			//	new_val = UNLOCK_NOT_A_LEAF(new_val, parent->container_pos);
			//	current = parent;
			//	parent = &(parent(current));
			//}
			////
					
			do{
				CHECK_BROTHER_OCCUPIED(p_pos,new_val); //questo termina il ciclo se il fratello è occupato e setta exit = true
				p_pos/=2;
				new_val = UNLOCK_NOT_A_LEAF(new_val, p_pos);
			}while(p_pos != 1);
			
		}while(new_val!=old_val && !__sync_bool_compare_and_swap(&(parent_ptr_by_ptr(parent).container->nodes), old_val, new_val));
	
	}while(BUNCHROOT(parent) != upper_bound && !do_exit);
	
	
	//if(current != upper_bound && !exit) //OCCHIO perchè current è la radice successiva
	//	smarca_(current); //devo andare su current che sarebbe la radice successiva;
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




/********** UNSED ***************/

//static unsigned long long occupa_discendenti(node* n, unsigned long long n_pos, unsigned long long new_val){//TODO
//	//se l'ultimo grappolo non ha tutti i nodi che deve avere (tipo ha solo due livelli). QUesto quando andiamo a regime non dovrebbe succedere perchè questa situazione la evitiamo (inutile avere un grappolo monco.. il numero di cas è lo stesso
//	if(left_index(n) >= number_of_nodes)//MAURO: non sono sicuro sia corretto...
//		return new_val;
//	if(!is_leaf_by_idx(lchild_idx_by_idx(n_pos))){
//		new_val = LOCK_NOT_A_LEAF(new_val, lchild_idx_by_idx(n_pos));// left(n).container_pos);
//		new_val = LOCK_NOT_A_LEAF(new_val, rchild_idx_by_idx(n_pos));//right(n).container_pos);
//		new_val = occupa_discendenti(&lchild_ptr_by_ptr(n)/*&left(n)*/, lchild_idx_by_idx(n_pos), new_val);
//		new_val = occupa_discendenti(&rchild_ptr_by_ptr(n)/*&right(n)*/, rchild_idx_by_idx(n_pos), new_val);//TODO: vediamo se si può eliminare la ricorsione
//	}
//	else{
//		new_val = LOCK_A_LEAF(new_val, lchild_idx_by_idx(n_pos));//left(n).container_pos);
//		new_val = LOCK_A_LEAF(new_val, rchild_idx_by_idx(n_pos));//right(n).container_pos);
//	}
//	return new_val;
//}


//static void smarca(node* n){
//#ifdef NUMA
//	tree = overall_trees[n->numa_node];
//	containers = overall_containers[n->numa_node];
//#endif
//	upper_bound = &ROOT;
//	smarca_(n);
//}





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



//MARK: WRITE SU FILE

/* 
	SCRIVE SU FILE LA SITUAZIONE DELL'ALBERO (IN AMPIEZZA) VISTA DA UN CERTO THREAD
 */

/*
static void write_on_a_file_in_ampiezza(){
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
		fprintf(f, "(%p) %llu\t val=%llu has %llu B. mem_start in %llu  level is %u\n", (void*)n, tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));
	}
	
	fclose(f);
#endif
}

static void write_on_a_file_in_ampiezza_start(){
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
		fprintf(f, "(%p) %llu val=%llu has %llu B. mem_start in %llu  level is %u\n", (void*)n, tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));
	}
	
	fclose(f);
#endif
}
*/

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
	unsigned int starting_node, count = 0, i;
	node * n;
 
	starting_node = number_of_leaves;
	
	for(i=0; i<number_of_leaves; i++){
		n = &(tree[starting_node+i]);
		if(!IS_ALLOCABLE(n->container->nodes, n->container_pos))
			count++;
	}	
	return count;
}

void write_on_a_file_in_ampiezza_start(unsigned int iter){
	char filename[128];
	int i;
#ifdef NUMA
	int actual_node;
	for(actual_node=0;actual_node<number_of_numa_nodes;actual_node++){
		tree = overall_trees[actual_node];
		printf("shampo\n");
		sprintf(filename, "./debug/init_tree%d_%d_%d.txt", actual_node, getpid(), iter);
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
	sprintf(filename, "./debug/4nb-init_tree%d.txt", iter);
	FILE *f = fopen(filename, "w");
	
	if (f == NULL){
		printf("Error opening file!\n");
		exit(1);
	}
	//fprintf(f, "%d \n", getpid());
	
	for(i=1;i<=number_of_nodes;i++){
		node* n = &tree[i];
		fprintf(f, "\t%d\t %5u val=%2lu has %8lu B. mem_start in %8lu  level is %2u\n",iter, tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));
	
	     if(level(n)!=overall_height && level(n)!= level(&tree[i+1]))
       fprintf(f,"\n");
     }
	
	fclose(f);
#endif
}
