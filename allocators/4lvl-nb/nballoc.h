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

#ifndef __NB_ALLOC__
#define __NB_ALLOC__

/****************************************************
				ALLOCATOR PARAMETES
****************************************************/

#ifndef MIN_ALLOCABLE_BYTES
#define MIN_ALLOCABLE_BYTES 8ULL //(2KB) numero minimo di byte allocabili
#endif
#ifndef MAX_ALLOCABLE_BYTES
#define MAX_ALLOCABLE_BYTES  16384ULL //(16KB)
#endif
#ifndef NUM_LEVELS
#define NUM_LEVELS  20ULL //(16KB)
#endif

#define PAGE_SIZE (4096)


typedef unsigned long long nbint; 


typedef struct _taken_list_elem{
	struct _taken_list_elem* next;
	node* elem;
}taken_list_elem;

typedef struct _taken_list{
	struct _taken_list_elem* head;
	unsigned int number;
}taken_list;


extern __thread unsigned int myid;
extern unsigned long long number_of_leaves;

void  bd_xx_free(void* n);
void* bd_xx_malloc(size_t pages);


#ifdef DEBUG
extern unsigned long long *node_allocated, *size_allocated;
#endif


#ifndef BD_SPIN_LOCK
	#define BD_LOCK_TYPE /**/
	#define INIT_BD_LOCK /**/
	#define BD_LOCK(x)   	 /**/
	#define BD_UNLOCK(x) 	 /**/
#endif

#endif
