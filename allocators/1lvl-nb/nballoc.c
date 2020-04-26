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
* Copyright (c) 2017 - 2020
* 
* 
* Romolo Marotta  (Contact author)
* Mauro Ianni
* Andrea Scarselli
* 
* 
* This file implements the non blocking buddy system (1lvl).
* 
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include "nb1lvl.h"
#include "utils.h"
#include <assert.h>



/*********************************************     
 *       MASKS FOR ACCESSING NODE BITMAPS
 *********************************************

  Bitmap for each node:

  32-----------5-----------4--------------------3--------------------2--------------1--------------0
  |  DONT CARE | OCCUPANCY | PENDING COALESCING | PENDING COALESCING | OCCUPANCY OF | OCCUPANCY OF |
  |            |           | OPS ON LEFT  CHILD | OPS ON RIGHT CHILD | LEFT CHILD   | RIGHT CHILD  |
  |------------|-----------|--------------------|--------------------|--------------|--------------|

 */

#define FREE_BLOCK                  ( 0x0U  )
#define MASK_OCCUPY_RIGHT           ( 0x1U  )
#define MASK_OCCUPY_LEFT            ( 0x2U  )
#define MASK_RIGHT_COALESCE         ( 0x4U  )
#define MASK_LEFT_COALESCE          ( 0x8U  )
#define OCCUPY                      ( 0x10U )

#define MASK_CLEAN_LEFT_COALESCE    (~MASK_LEFT_COALESCE)
#define MASK_CLEAN_RIGHT_COALESCE   (~MASK_RIGHT_COALESCE)
#define OCCUPY_BLOCK                ((OCCUPY) | (MASK_OCCUPY_LEFT) | (MASK_OCCUPY_RIGHT))
#define MASK_CLEAN_OCCUPIED_LEFT    (~MASK_OCCUPY_LEFT )
#define MASK_CLEAN_OCCUPIED_RIGHT   (~MASK_OCCUPY_RIGHT)

#define ROOT                        (tree[1])

#define lchild_idx_by_idx(n)        (n << 1)
#define rchild_idx_by_idx(n)        (lchild_idx_by_idx(n)+1)
#define parent_idx_by_idx(n)        (n >> 1)

#define is_left_by_idx(n)           (1ULL & (~(n)))

#define level_by_idx(n)             (1 + (log2_(n)))

#define NUMBER_OF_NODES             ((1 <<  NUM_LEVELS) -1 )
#define NUMBER_OF_LEAVES            ( 1 << (NUM_LEVELS  -1))


/***************************************************
 *               NBBS VARIABLES
 **************************************************/


#ifdef BD_SPIN_LOCK
BD_LOCK_TYPE glock;
#endif

#ifdef DEBUG
nbint  *size_allocated;
unsigned long long *node_allocated;
#endif

__thread unsigned int tid=-1;
unsigned int partecipants=0;

static node* volatile tree                      = NULL;
static node* volatile free_tree                 = NULL;
static void* volatile overall_memory            = NULL;

static unsigned long long overall_memory_size   = (MIN_ALLOCABLE_BYTES * NUMBER_OF_LEAVES);
static unsigned long long number_of_nodes       = NUMBER_OF_NODES;
static unsigned long long number_of_leaves      = NUMBER_OF_LEAVES;
static unsigned long long overall_height        = NUM_LEVELS;
static unsigned long long max_level             = 0ULL;
static volatile int init_phase                  = 0;


/***************************************************
 *               NBBS PRIVATE PROCEDURES
 **************************************************/

static void init_tree(unsigned long number_of_nodes);
static unsigned long long alloc(unsigned long long, unsigned long long);
static void internal_free_node(unsigned long long n, unsigned long long upper_bound);


/*******************************************************************
 *               INIT NBBS
 ******************************************************************/

void __attribute__ ((constructor(500))) premain(){ init(); }

/*
 This function build the Non-Blocking Buddy System.
 */
void init(){
    void *tmp_overall_memory;
    void *tmp_tree;
    void *tmp_free_tree;
    bool first = false;
    
    if(overall_memory_size < MAX_ALLOCABLE_BYTE) NB_ABORT("No enough levels\n");
    
    max_level = overall_height - log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES);        //last valid allocable level
    
    if(init_phase ==  0 && __sync_bool_compare_and_swap(&init_phase, 0, 1)){

        tmp_overall_memory = mmap(NULL, overall_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        if(tmp_overall_memory == MAP_FAILED) 
            NB_ABORT("No enough levels\n");
        else if(!__sync_bool_compare_and_swap(&overall_memory, NULL, tmp_overall_memory))
            munmap(tmp_overall_memory, overall_memory_size);
            
        tmp_tree = mmap(NULL,64+(1+number_of_nodes)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        
        if(tmp_tree == MAP_FAILED)      
            abort();
        else if(!__sync_bool_compare_and_swap(&tree, NULL, tmp_tree)) 
            munmap(tmp_tree, 64+(1+number_of_nodes)*sizeof(node));

        tmp_free_tree = mmap(NULL,64+(number_of_leaves)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        if(tmp_free_tree == MAP_FAILED) 
            abort();
        else if(!__sync_bool_compare_and_swap(&free_tree, NULL, tmp_free_tree)) 
            munmap(tmp_free_tree, 64+(number_of_leaves)*sizeof(node));

#ifdef DEBUG
    node_allocated = mmap(NULL, sizeof(unsigned long long), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    size_allocated = mmap(NULL, sizeof(nbint), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      
    __sync_fetch_and_and(node_allocated,0);
    __sync_fetch_and_and(size_allocated,0);
    
    printf("Debug mode: ON\n");
#endif

        __sync_bool_compare_and_swap(&tree, NULL, tmp_tree);
        __sync_bool_compare_and_swap(&free_tree, NULL, tmp_free_tree);
        __sync_bool_compare_and_swap(&init_phase, 1, 2);
    }

    while(init_phase < 2);

    if(init_phase == 2) 
        init_tree(number_of_nodes);
    else
        return;

    
    if(first){
#ifdef BD_SPIN_LOCK
        printf("1lvl-sl: UMA Init complete\n");
#else
        printf("1lvl-nb: UMA Init complete\n");
#endif
        printf("\t Total Memory = %lluB, %.0fKB, %.0fMB, %.0fGB\n" , overall_memory_size, overall_memory_size/1024.0, overall_memory_size/1048576.0, overall_memory_size/1073741824.0);
        printf("\t Levels = %llu\n", overall_height);
        printf("\t Leaves = %10llu\n", (number_of_nodes+1)/2);
        printf("\t Nodes  = %10llu\n", number_of_nodes);
        printf("\t Min size %12lluB, %.0fKB, %.0fMB, %.0fGB at level %llu\n"   , MIN_ALLOCABLE_BYTES, MIN_ALLOCABLE_BYTES/1024.0, MIN_ALLOCABLE_BYTES/1048576.0, MIN_ALLOCABLE_BYTES/1073741824.0, overall_height);
        printf("\t Max size %12lluB, %.0fKB, %.0fMB, %.0fGB at level %llu\n"   , MAX_ALLOCABLE_BYTE, MAX_ALLOCABLE_BYTE/1024.0, MAX_ALLOCABLE_BYTE/1048576.0, MAX_ALLOCABLE_BYTE/1073741824.0, overall_height - log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES));
        printf("\t Max allocable level %2llu\n", max_level);
    }
}

/*
 This function inits a static tree represented as an implicit binary heap. 
 The first node at index 0 is a dummy node.
 */
static void init_tree(unsigned long number_of_nodes){
    int i=0;

    ROOT.val = 0ULL;
    for(i=2;i<=number_of_nodes;i++) tree[i].val = 0ULL;

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
 This function destroy the Non-Blocking Buddy System.
 */
void destroy(){
    free(overall_memory);
    free(free_tree);
    free(tree);
}

/*
 API for memory allocation.
 */
void* bd_xx_malloc(size_t byte){
    unsigned long long starting_node, last_node, actual, started_at, failed_at_node;
    unsigned long long leaf_position;
    unsigned long long searched_lvl = 0;
    bool restarted = false;

    // just on startup 
    if(tid == -1)  
        tid = __sync_fetch_and_add(&partecipants, 1);

    // check memory request size
    if( byte > MAX_ALLOCABLE_BYTE || byte > overall_memory_size)   
        return NULL;  

    // round to a proper size
    byte = upper_power_of_two(byte);
    if( byte < MIN_ALLOCABLE_BYTES ) 
        byte = MIN_ALLOCABLE_BYTES;

    // first node for this level
    starting_node  = overall_memory_size / byte;
    
    // last node for this level
    last_node      = lchild_idx_by_idx(starting_node)-1;

    // get the target level
    searched_lvl   = level_by_idx(starting_node);

    // check local cache level
    actual         = get_freemap(searched_lvl, last_node);
    if(!actual)    actual = started_at = starting_node + (tid) * ((last_node - starting_node + 1)/partecipants);
    
    // start index
    started_at = actual;
    
    do{
        // try to allocate the target node 
        // uses locks in the blocking version 
        BD_LOCK(&glock);
        failed_at_node = alloc(actual, searched_lvl);
        BD_UNLOCK(&glock);
 
        // successful allocation
        if(failed_at_node == 0){
            
          #ifdef DEBUG
            __sync_fetch_and_add(node_allocated,1);
            __sync_fetch_and_add(size_allocated,byte);
          #endif
            // get the position of the minimum-index leaf in the allocated block
            leaf_position = byte*(actual - overall_memory_size / byte)/MIN_ALLOCABLE_BYTES;
            // set up translation table
            free_tree[leaf_position].val = actual;
            // populate cache assuming that the nest node will be free
            update_freemap(searched_lvl, starting_node+((actual+1)%starting_node));
            return ((char*) overall_memory) + leaf_position*MIN_ALLOCABLE_BYTES;
        }
        
        // failed while fragmenting a higher-order node so skip nodes surely occupied 
        actual = (failed_at_node + 1) * (1 << (        searched_lvl - level_by_idx(failed_at_node)));
        
        // the last node of the target level has been reached restart from the first node 
        if(actual > last_node){
            actual = starting_node;
            restarted = true;
        }
        // all nodes have been checked
    }while(restarted == false || actual < started_at);
    
    return NULL;
}


/*
 This routine implements the actual allocation.
 It sets the target node has BUSY, then it marks the occupancy bit of the respective child for each ancestor of the allocated node.
 This routine returns 0 if it succeeds to mark every bit from the allocated node to the root, 
 otherwise it returns the index of the node that made the allocation fail.
 */
static unsigned long long alloc(unsigned long long n, unsigned long long lvl){
    unsigned long long actual_value;
    unsigned long long failed_at_node;
    unsigned long long new_value;
    bool is_left_child;
    unsigned long long actual = n;
    
    // get tha state of the target node
    actual_value = tree[n].val;
    
    // try to allocate the node
    if(actual_value != 0 || !__sync_bool_compare_and_swap(&(tree[n].val),0,OCCUPY_BLOCK)) return n;
        
    while(lvl != max_level){
    
        // parent node
        lvl--;
        is_left_child = is_left_by_idx(actual);
        actual = parent_idx_by_idx(actual);

        // retry loop for fragmenting it
        do{
            actual_value = tree[actual].val;
            
            // check if parent has been fully occupied
            if((actual_value & OCCUPY)!=0){
                failed_at_node = actual;
                // we need to rollback the work done before failing
                internal_free_node(n, lvl+1);
                return failed_at_node;
            }
            
            // flip bits of the state correctly
            new_value = actual_value;
            if(is_left_child)   new_value = (new_value & MASK_CLEAN_LEFT_COALESCE ) | MASK_OCCUPY_LEFT;
            else                new_value = (new_value & MASK_CLEAN_RIGHT_COALESCE) | MASK_OCCUPY_RIGHT;
            
            
            // if we are using the lock simply write otherwise go for a CAS
            #ifdef BD_SPIN_LOCK  
                tree[actual].val = actual_value = new_value;
            #endif
        }while(new_value != actual_value && 
                !__sync_bool_compare_and_swap(&(tree[actual].val), actual_value, new_value));
    }
    return 0;
}




/*
 This function cleans both coalescing and occupancy bit of the anchestor of the node passed as parameter
 It might leave some coalescing bit set, but it is not a real issue (also allocations can clean them).
 */

static inline void unmark(unsigned long long n, unsigned long long upper_bound){
    unsigned long long actual_value;
    unsigned long long new_val;
    bool is_left_child;
    unsigned long long actual = n;
    unsigned long long lvl = level_by_idx(actual);
    
    do{
        
        // get parent node
        lvl--;
        is_left_child = is_left_by_idx(actual);
        actual = parent_idx_by_idx(actual);

        do{
            actual_value = tree[actual].val;
            new_val = actual_value;
            
            // if bits have been already cleaned by a concurrent allocation we can return
            if( (actual_value & (MASK_RIGHT_COALESCE << is_left_child) ) == 0  )    return;

            // compute the new value 
            if(is_left_child)   new_val = new_val & ((MASK_CLEAN_LEFT_COALESCE & MASK_CLEAN_OCCUPIED_LEFT));
            else                new_val = new_val & ((MASK_CLEAN_RIGHT_COALESCE & MASK_CLEAN_OCCUPIED_RIGHT));
            
            #ifdef BD_SPIN_LOCK  
                // we have a lock so a simple write is enough 
                tree[actual].val = actual_value = new_val;
            #endif
          // go for a cas 
        } while (new_val != actual_value && !__sync_bool_compare_and_swap(&(tree[actual].val),actual_value,new_val));
    }while( (lvl != upper_bound) &&
            !( (new_val & (MASK_OCCUPY_LEFT >> is_left_child) ) != 0 )  

            );

}

/*
 This routine is the actual implementation of the memory release.
 It works in three phases:
   1. Mark ancestors of the to-be-freed node as coalescent 
   2. Mark the target node as free
   3. Clean coalescent bits of the anchestors
 */
static inline void internal_free_node(unsigned long long n, unsigned long long upper_bound){
    unsigned long long actual;
    unsigned long long runner, lvl;
    unsigned long long old_val;
    bool is_left_child;

    assert(tree[n].val == OCCUPY_BLOCK);
    if( tree[n].val != OCCUPY_BLOCK ){
        printf("err: il blocco non è occupato\n");
        return;
    }
    
    actual = parent_idx_by_idx(n);
    runner = n;
    lvl = level_by_idx(runner);
        
    while(lvl != upper_bound){
        old_val = __sync_fetch_and_or(&(tree[actual].val),  (MASK_RIGHT_COALESCE << (lchild_idx_by_idx(actual)==runner) ) ) ;
        is_left_child = is_left_by_idx(runner);
        
        if( ((old_val & (MASK_OCCUPY_LEFT >> is_left_child)) != 0 ) &&
            ((old_val & (MASK_LEFT_COALESCE >> is_left_child)) == 0 )){
                break;
        }
        
        runner = actual;
        actual = parent_idx_by_idx(actual);
        lvl--;
    }
    
    tree[n].val = 0; // TODO aggiungi barriera --- secondo me "__sync_lock_release" va bene
    if(n!=upper_bound)  unmark(n, upper_bound);
}

/*
 API for memory release.
 */
void bd_xx_free(void* n){
    // Obtain the leaf corresponding to the address
    unsigned long long tmp = ((unsigned long long)n) - (unsigned long long)overall_memory;
    unsigned long long pos = (unsigned long long) tmp;

    // Use the leaf position to obtain the allocated node 
    pos = pos / MIN_ALLOCABLE_BYTES;
    pos = free_tree[pos].val;

    // update local cache 
    update_freemap(level_by_idx(pos), pos);

    // start actual release of the memory block 
    BD_LOCK(&glock);
    internal_free_node(pos, max_level);
    BD_UNLOCK(&glock);
#ifdef DEBUG
    __sync_fetch_and_add(node_allocated,-1);
    __sync_fetch_and_add(size_allocated,-(n->mem_size));
#endif
}
