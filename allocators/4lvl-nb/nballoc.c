/**		      
* This is free software; 
* You can redistribute it and/or modify this file under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* This file is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* this file; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* Copyright (c) 2017
* 
* 
* Romolo Marotta 
* Mauro Ianni
* Andrea Scarselli
* 
* 
* This file implements the non blocking buddy system (1lvl).
* 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/wait.h>
#include "nb4lvl.h"
#include "utils.h"


/*********************************************	 
*	   MASKS FOR ACCESSING CONTAINER BITMAPS
*********************************************

Bitmap for container:
 48 		 43    		 38          33           28          23		 18          13		      8  7  6  5  4  3  2  1  0	
  |-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|--|--|--|--|--|--|--|--|
  | 15th NODE | 14th NODE | 13th NODE | 12th NODE | 11th NODE | 10th NODE |  9th NODE |  8th NODE |   ONE OCCUPANCY BIT   |
  |-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|--|--|--|--|--|--|--|--|
                                                                                            |
                                                                                            |
                                                                                            |
    	     12				      11		           10              9              8	    |	
  |-----------|--------------------|--------------------|--------------|--------------|     |
  | OCCUPANCY | PENDING COALESCING | PENDING COALESCING | OCCUPANCY OF | OCCUPANCY OF |<----|
  |		      | OPS ON LEFT  CHILD | OPS ON RIGHT CHILD | LEFT CHILD   | RIGHT CHILD  |
  |-----------|--------------------|--------------------|--------------|--------------|

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

#define bunchroot_idx_by_idx_and_lvl(n, lvl) ( (n) >> ( (lvl) & 3ULL) )

//PARAMETRIZZAZIONE
#define LEVEL_PER_CONTAINER 4

/* VARIABILI GLOBALI *//*---------------------------------------------------------------------------------------------*/

static node *tree; //array che rappresenta l'albero, tree[0] è dummy! l'albero inizia a tree[1].
static node *free_tree = NULL;
static node_container* containers; //array di container In Numa questo sarà uno specifico tree (di base tree[getMyNUMANode])
unsigned long long overall_memory_size;
unsigned long long overall_memory_pages;
unsigned long long overall_height;
static unsigned int max_level; //Ultimo livello utile per un allocazione
unsigned int number_of_leaves;
unsigned int number_of_nodes; //questo non tiene presente che tree[0] è dummy! se qua c'è scritto 7 vuol dire che ci sono 7 nodi UTILIZZABILI
//unsigned int number_of_processes;
unsigned int number_of_container;
static volatile unsigned long long levels = NUM_LEVELS;
void* overall_memory;

#ifdef DEBUG
unsigned long long *node_allocated, *size_allocated;
#endif

#ifdef BD_SPIN_LOCK
BD_LOCK_TYPE glock;
#endif


__thread unsigned int tid=-1;
unsigned int partecipants=0;

/* DICHIARAZIONE DI FUNZIONI *//*---------------------------------------------------------------------------------------------*/

static void init_tree(unsigned long long number_of_nodes);
static unsigned long long alloc(unsigned long long n);
static void marca(unsigned long long n, unsigned int upper_bound);
static bool IS_OCCUPIED(unsigned long long, unsigned);
static unsigned long long check_parent(unsigned long long n);
static void smarca(unsigned long long n, unsigned int upper_bound);
static void internal_free_node(unsigned long long n, unsigned int upper_bound);
void* bd_xx_malloc(size_t pages);



/* FUNZIONI *//*---------------------------------------------------------------------------------------------*/

//MARK: INIT
/*
 Questa funzione inizializza l'albero. Non il nodo dummy (tree[0])
 @param number_of_nodes: the number of nodes.
 */
static void init_tree(unsigned long long number_of_nodes){
	unsigned long long i=0;

	number_of_container = 0;
	
	ROOT.mem_start = 0ULL;
	ROOT.mem_size = overall_memory_size;
	ROOT.pos = 1ULL;
	ROOT.container = &containers[number_of_container++];
	ROOT.container_pos = 1ULL;
	ROOT.container->bunch_root = &ROOT;
    ROOT.container->nodes = 0ULL;
	for(i=2;i<=number_of_nodes;i++){
		tree[i].pos = i;
		node parent = parent_ptr_by_ptr(&tree[i]);
		tree[i].mem_size = (parent.mem_size) / 2;
		tree[i].mem_start = (parent.mem_start);
		if(i%2!=0)
			tree[i].mem_start += tree[i].mem_size;
		
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
	#ifdef BD_SPIN_LOCK
    #if BD_SPIN_LOCK == 0
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(glock), &attr);
    #else
    pthread_spin_init(&(glock), PTHREAD_PROCESS_SHARED);
    #endif
    #endif
}


/*
	@param pages: pagine richieste. Questo sarà il valore della radice
 */
static void init(){
	number_of_nodes = (1<<levels) - 1;
	
	number_of_leaves = (1<< (levels-1));
	overall_memory_size = MIN_ALLOCABLE_BYTES * number_of_leaves;
	overall_height = levels;
	max_level = overall_height - log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES); //last valid allocable level
	max_level = ((unsigned long long)((max_level-1)/4))*4 + 1;//max_level - max_level%4 + 1;  


	overall_memory 	= mmap(NULL, overall_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	tree 			= mmap(NULL,(1+number_of_nodes)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	containers 		= mmap(NULL,(number_of_nodes-1)*sizeof(node_container), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	free_tree  		= mmap(NULL,64+(number_of_leaves)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
     
	if(overall_memory==MAP_FAILED || tree==MAP_FAILED || containers==MAP_FAILED || free_tree==MAP_FAILED){
		puts("Failing allocating structures\n");
		abort();
	}
	
	init_tree(number_of_nodes);
		
	printf("4lvl-nb: UMA Init complete\n");
	printf("\t Total Memory = %llu\n", overall_memory_size);
	printf("\t Levels = %10llu\n", overall_height);
	printf("\t Leaves = %10u\n", (number_of_nodes+1)/2);
	printf("\t Nodes  = %10u\n", number_of_nodes);
	printf("\t Containers = %u\n", number_of_container);
	printf("\t Min size %12llu at level %2llu\n", MIN_ALLOCABLE_BYTES, overall_height);
	printf("\t Max size %12llu at level %2llu\n", MAX_ALLOCABLE_BYTE, overall_height - log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES));
	printf("\t Max allocable level %2u\n", max_level);
	
}

__attribute__((constructor(500))) void pre_init() {
	init();	
}

__attribute__((destructor)) void end(){
	//free(overall_memory);
	//free(containers);
	//free(tree);
#ifdef DEBUG
	//free(node_allocated);
	//free(size_allocated);
#endif
}


//MARK: ALLOCAZIONE

/*
 Questa funziona controlla se il nodo in posizione container_pos è TOTALMENTE occupato (se si tratta di una foglia) oppure se è parzialmente o totalmente occupato se non è una foglia, nella maschera dei nodi rappresentata da val. Ricordo che per la semantica dell'algoritmo una foglia è totalmente occupata se e solo ha il quinto bit settato.
 @param val: il container nel quale il nodo si trova
 @param pos: il container_pos del nodo che vogliamo controllare
 
 */
static inline bool IS_OCCUPIED(unsigned long long val, unsigned int pos){
	if(pos < LEAF_START_POSITION)
		return ( (val & ((LOCK_NOT_LEAF_MASK) << (pos - 1))) != 0);
	else
		return val & (( (LOCK_NOT_LEAF_MASK) << (LEAF_START_POSITION - 2)) << (5* (pos - (LEAF_START_POSITION - 1))));
}


/*
 Funzione di malloc richiesta dall'utente.
 @param pages: memoria richiesta dall'utente
 @return l'indirizzo di memoria del nodo utilizzato per soddisfare la richiesta; NULL in caso di fallimento
 */
void* bd_xx_malloc(size_t byte){
	bool restarted = false; 
	unsigned long long started_at, actual, starting_node, last_node, failed_at, leaf_position;
	
    if(tid == -1){
		tid = __sync_fetch_and_add(&partecipants, 1);
    }
	
	if(byte > MAX_ALLOCABLE_BYTE)
		return NULL;
		
	byte = upper_power_of_two(byte);
	
	if(byte < MIN_ALLOCABLE_BYTES)
		byte = MIN_ALLOCABLE_BYTES;
	
	starting_node = overall_memory_size / byte; //first node for this level
	last_node = lchild_idx_by_idx(starting_node)-1;//last node for this level
	
	
	//actual è il posto in cui iniziare a cercare
actual = get_freemap(level_by_idx(starting_node), last_node);
if(!actual)	actual = started_at = starting_node + (tid) * ((last_node - starting_node + 1)/partecipants);
	//actual = started_at = starting_node + (myid) * ((last_node - starting_node + 1)/number_of_processes);
    started_at = actual;
	//quando faccio un giro intero ritorno NULL
	do{
  	    BD_LOCK(&glock);
		failed_at = alloc(actual);
	    BD_UNLOCK(&glock);     
		if(failed_at == 0)
		{
#ifdef DEBUG
			__sync_fetch_and_add(node_allocated,1);
			__sync_fetch_and_add(size_allocated,byte);
#endif
            leaf_position = byte*(actual - overall_memory_size / byte)/MIN_ALLOCABLE_BYTES;
            free_tree[leaf_position].pos = actual;
            //printf("leaf pos %d\n", leaf_position);
            return ((char*) overall_memory) + leaf_position*MIN_ALLOCABLE_BYTES; //&tree[actual]
		}

		//Questo serve per evitare tutto il sottoalbero in cui ho fallito
		actual = (failed_at + 1) * (1 << ( level_by_idx(actual) - level_by_idx(failed_at) ) );
		
		if(actual > last_node){ //se ho sforato riparto dal primo utile, se il primo era quello da cui avevo iniziato esco al controllo del while
			actual = starting_node;
			restarted = true;
		}
	}while(restarted == false || actual < started_at);
	
	return NULL;
}

/*
	Questa è una funzione di help per la alloc. Occupa tutti i discendenti del nodo n presenti nello stesso grappolo. NB questa funzione modifica solo new_val; non fa CAS, la modifica deve essere apportata dal chiamante.
	@param n: il nodo (OCCUPATO) a cui occupare i discendenti
	@param new_val: sarebbe  n->container->nodes da modificare
 
 */
static inline unsigned long long occupa_container(unsigned long long n_pos, unsigned long long new_val){
	unsigned long long i=0, tmp_val = 1ULL, starting, ending, curr_pos;
	
	//Scrivo i nodi da n (escluso) fino alla radice
	curr_pos = n_pos;
	while((curr_pos/=2) != 0){
		new_val = LOCK_NOT_A_LEAF(new_val, curr_pos); 
	}
	
	//Scrivo i nodi da n (incluso) fino alle foglie
	curr_pos = n_pos;	
	while(curr_pos < LEAF_START_POSITION){
		i++;
		new_val |= (tmp_val << (curr_pos-1));
		tmp_val |= (tmp_val << (curr_pos/n_pos));
		curr_pos *= 2;
	}
	
	//Scrivo sulle foglie
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
	if(pos < LEAF_START_POSITION)
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
static unsigned long long alloc(unsigned long long n){
	unsigned long long old_val, new_val, n_pos, n_bpos, n_lvl;
	unsigned long long * volatile container;
	n_pos = n;
	n_lvl = level_by_idx(n_pos);
	container = &tree[n].container->nodes;
	n_bpos = tree[n].container_pos;
	do{
		new_val = old_val = *container;
		
		if(!IS_ALLOCABLE(new_val, n_pos)){
			return n_pos;
		}
		
		new_val = occupa_container(n_pos, new_val);
		#ifdef BD_SPIN_LOCK
			*container = old_val = new_val;
		#endif
	}while(new_val!=old_val && !__sync_bool_compare_and_swap(container, old_val, new_val));
	
	//if(n->container->bunch_root == &ROOT){
	if( level_by_idx(bunchroot_idx_by_idx_and_lvl(n_pos, n_lvl))  <= max_level){
		return 0;
	}

	return check_parent(n);
}


/*
 Questa funzione si preoccupa di marcare il bit relativo al sottoalbero del blocco che ho allocato in tutti i suoi antenati.
 Questa funziona ritorna true se e solo se riesce a marcare fino alla radice. La funzione fallisce se e solo se si imbatte in un nodo occupato (ricordando che se un generico nodo è occupato, allora la foglia ad esso relativa (il nodo stesso o un suo discendente) avrà il falore 0x13)
 La funzione si preoccupa inoltre di cancellare l'eventuale bit di coalescing.
 Side effect: se fallisce la variabile globale failed_at_node assume il valore del nodo dove la ricorsione è fallita (TODO: occhio, fallisco sulla foglia, magari il nodo bloccato era il padre, non ho modo di saperlo, rischio di sbattare nuovamente su questo nodo)
 @param n: nodo da cui iniziare la risalita (che è già marcato totalmente o parzialmente). Per costruzione della alloc, n è per forza un nodo radice di un grappolo generico (ma sicuramente non la radice)
 @return true se la funzione riesce a marcare tutti i nodi fino alla radice; false altrimenti.
 */
static unsigned long long check_parent(unsigned long long nidx){
	unsigned long long new_val, old_val, tmp_container_pos, p_pos;
	unsigned long long br_idx, n_lvl;
	unsigned long long *container;
	node* parent;
	n_lvl = level_by_idx(nidx);
	do{
		br_idx = bunchroot_idx_by_idx_and_lvl(nidx, n_lvl);
		parent = &parent_ptr_by_idx(br_idx);
		p_pos  = parent->container_pos; 
		
		do{
			tmp_container_pos = p_pos; 
			container = &parent->container->nodes;
			new_val = old_val = *container;
			
			if(IS_OCCUPIED(old_val, tmp_container_pos)){
				internal_free_node(nidx, level_by_idx(br_idx));
				return nidx;
			}
			
			if(is_left_by_idx(br_idx)){
				new_val = CLEAN_LEFT_COALESCE(new_val, tmp_container_pos);
				new_val = OCCUPY_LEFT(new_val, tmp_container_pos);
			}else{
				new_val = CLEAN_RIGHT_COALESCE(new_val, tmp_container_pos);
				new_val = OCCUPY_RIGHT(new_val, tmp_container_pos);
			}
			
			while((tmp_container_pos/=2) != 0){
				new_val = LOCK_NOT_A_LEAF(new_val, tmp_container_pos);
			}
			#ifdef BD_SPIN_LOCK
				*container = old_val = new_val;
			#endif

		}while(new_val!=old_val && !__sync_bool_compare_and_swap(container,old_val,new_val)); 
		nidx = parent_idx_by_idx(br_idx);
		n_lvl = level_by_idx(nidx);
	}while(n_lvl > max_level);	
	
	return 0;
}

//MARK: FREE


static inline unsigned long long libera_container(unsigned long long n_pos, unsigned long long new_val, bool *do_exit_ptr){//TODO
	unsigned long long i=0, tmp_val=1ULL, starting, ending, curr_pos;
	bool do_exit = false;
	
	curr_pos = n_pos;
	while(curr_pos > 1){
			CHECK_BROTHER_OCCUPIED(curr_pos,new_val); //questo termina il ciclo se il fratello è occupato e setta exit = true
			curr_pos/=2;
			new_val = UNLOCK_NOT_A_LEAF(new_val, curr_pos);
	}
		
	curr_pos = n_pos;
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
	*do_exit_ptr = do_exit;
	return new_val;
}


void bd_xx_free(void* n){
    unsigned long long tmp = ((unsigned long long)n) - (unsigned long long)overall_memory;
    unsigned int pos = (unsigned long long) tmp;
    pos = pos / MIN_ALLOCABLE_BYTES;
    pos = free_tree[pos].pos;
    update_freemap(level_by_idx(pos), pos);
    BD_LOCK(&glock);
    internal_free_node(pos, max_level);
	BD_UNLOCK(&glock);

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
static void internal_free_node(unsigned long long pos, unsigned int upper_bound){
	unsigned long long old_val, new_val, n_pos;
	bool do_exit = false;
	node* n = &tree[pos];
	unsigned long long n_lvl 	= level_by_idx(pos); //n->pos;
	unsigned long long br_idx   = bunchroot_idx_by_idx_and_lvl(pos, n_lvl);
	unsigned long long br_lvl   = level_by_idx(br_idx);
	unsigned long long *container = &n->container->nodes;
	
	
	// FASE 1
	if(br_lvl > upper_bound)//TODO: sistemare marca per togliere il controllo qui
		marca(br_idx, upper_bound);
	// FASE 2
	do{
		n_pos = n->container_pos; 
		old_val = new_val = *container;
		new_val = libera_container(n_pos, new_val, &do_exit);
		#ifdef BD_SPIN_LOCK
		*container = old_val = new_val;
		#endif
	}while(new_val!=old_val && !__sync_bool_compare_and_swap(container,old_val, new_val));
	
	// FASE 3
	if(br_lvl > upper_bound && !do_exit)
		smarca(br_idx, upper_bound);
}


/*
 Funzione ausiliaria a internal_free_node (in particolare al primo step della free). In questa fase la free vuole marcare tutti i nodi dei sottoalberi interessati dalla free come "coalescing". Qui n è la radice di un grappolo. Bisogna mettere il bit di coalescing al padre (che quindi sarà una foglia con 5 bit). Nota che questa funzione non ha ragione di esistere se il nodo da liberare è nel grappolo di upper_bound.
 @param n è la radice di un grappolo. Bisogna settare in coalescing il padre.
 @return il valore precedente con un singolo nodo marcato come "coalescing"
 */
static void marca(unsigned long long n_idx, unsigned int upper_bound){
	node* parent;
	unsigned long long old_val, new_val, p_pos;
	bool is_left_son;
	unsigned long long n_lvl = level_by_idx(n_idx);
	unsigned long long br_idx = bunchroot_idx_by_idx_and_lvl(n_idx, n_lvl);
	unsigned long long *container;
	do{
		is_left_son = is_left_by_idx(br_idx); 
		parent = &parent_ptr_by_idx(br_idx);
		p_pos = parent->container_pos;
		container = &parent->container->nodes;

		do{
			old_val = new_val = *container;
			new_val = new_val | (COALESCE_RIGHT(0, p_pos) << is_left_son);
			if(new_val==old_val)										//SPAA2018
				return;													//SPAA2018
			#ifdef BD_SPIN_LOCK
			*container = old_val = new_val;
			#endif
		}while(old_val != new_val && !__sync_bool_compare_and_swap(container, old_val, new_val));
//		}while(new_val!=old_val && !__sync_bool_compare_and_swap(&parent->container->nodes, old_val, new_val));
		
		n_idx  = parent_idx_by_idx(br_idx);
		n_lvl  = level_by_idx(n_idx);
		br_idx = bunchroot_idx_by_idx_and_lvl(n_idx, n_lvl);		
	}while(level_by_idx(br_idx) > upper_bound);
}


/*
 Questa funziona leva il bit di coalesce e libera il sottoramo (fino all'upper bound) ove necessario. Nota che potrebbe terminare lasciando alcuni bit di coalescing posti ad 1. Per i dettagli si guardi la documentazione. Upper bound è l'ultimo nodo da smarcare. n è la radice di un grappolo. Va pulito il coalescing bit al padre di n (che è su un altro grappolo). Quando inizio devo essere certo che parent(n) abbia il coalescing bit settato ad 1.
		La funzione termina prima se il genitore del nodo che voglio liberare ha l'altro nodo occupato (sarebbe un errore liberarlo)
 @param n: n è la radice di un grappolo (BISOGNA SMARCARE DAL PADRE)
 
 */
static void smarca(unsigned long long n, unsigned int upper_bound){
	unsigned long long old_val, new_val, p_pos;
	bool do_exit=false, is_left_son;
	node *parent;
	unsigned long long n_idx = n;
	unsigned long long n_lvl = level_by_idx(n_idx);
	unsigned long long br_idx = bunchroot_idx_by_idx_and_lvl(n_idx, n_lvl);
	unsigned long long *container;

	do{
		is_left_son = is_left_by_idx(br_idx); 
		parent = &parent_ptr_by_idx(br_idx);
		p_pos = parent->container_pos;
		container = &parent->container->nodes;			
		do{
			do_exit = false;
			
			old_val = new_val = *container;
			
			if(is_left_son){
				if(!IS_COALESCING_LEFT(new_val, p_pos)) //qualcuno l'ha già pulito
					return;

				new_val = CLEAN_LEFT_COALESCE(new_val, p_pos); //lo facciamo noi
				new_val = CLEAN_LEFT(new_val, p_pos);
				
				//SE il fratello è occupato vai alla CAS
				if(IS_OCCUPIED_RIGHT(new_val, p_pos)){
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
			
			do{
				CHECK_BROTHER_OCCUPIED(p_pos,new_val); //questo termina il ciclo se il fratello è occupato e setta exit = true
				p_pos/=2;
				new_val = UNLOCK_NOT_A_LEAF(new_val, p_pos);
			}while(p_pos != 1);									
			#ifdef BD_SPIN_LOCK
			parent_ptr_by_ptr(parent).container->nodes = old_val = new_val;
			#endif
		}while(new_val!=old_val && !__sync_bool_compare_and_swap(&(parent_ptr_by_ptr(parent).container->nodes), old_val, new_val));
		
		n_idx = br_idx;
		n_lvl = level_by_idx(br_idx);
		br_idx = bunchroot_idx_by_idx_and_lvl(n_idx, n_lvl);
	}while(level_by_idx(br_idx) > upper_bound && !do_exit);
	
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
/*
void write_on_a_file_in_ampiezza(unsigned int iter){
	char filename[128];
	int i;

	sprintf(filename, "./debug/%u - tree%d.txt", iter, getpid());
	FILE *f = fopen(filename, "w");
	
	if (f == NULL){
		printf("Error opening file!\n");
		exit(1);
	}
	//fprintf(f, "%d \n", getpid());
	
	for(i=1;i<=number_of_nodes;i++){
		node* n = &tree[i];
		fprintf(f, "(%p) %5llu val=%2llu has %8llu B. mem_start in %8llu  level is %2u\n", (void*)n, tree[i].pos,  VAL_OF_NODE(n) , tree[i].mem_size, tree[i].mem_start,  level(n));
		if(level(n)!=overall_height && level(n)!= level(&tree[i+1]))
			fprintf(f,"\n");
	}
	
	fclose(f);

}

static void write_on_a_file_in_ampiezza_start(){
	char filename[128];
	int i;
	sprintf(filename, "./debug/init_tree%d.txt", getpid());
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
/*
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
*/

