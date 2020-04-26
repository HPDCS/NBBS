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

#ifndef __NB_ALLOC__
#define __NB_ALLOC__


/****************************************************
                ALLOCATOR PARAMETERS
****************************************************/

#ifndef MIN_ALLOCABLE_BYTES                     // Minimum size for allocation
#define MIN_ALLOCABLE_BYTES 4096ULL             // Default value
#endif

#ifndef MAX_ALLOCABLE_BYTE                      // Maximum size for allocation
#define MAX_ALLOCABLE_BYTE  (4096ULL*1024ULL)   // Default value 
#endif

#ifndef NUM_LEVELS                              // Number of levels of the tree
#define NUM_LEVELS          12ULL               // Default value 
#endif

//#define DEBUG

void  bd_xx_free(void* n);                  // Release API
void* bd_xx_malloc(size_t bytes);           // Alloc   API
void  init();                               // Init    API

#ifdef DEBUG
extern unsigned long long *node_allocated;  // Additional variable for debugging
extern nbint *size_allocated;               // Additional variable for debugging
#endif

#ifndef BD_SPIN_LOCK                        // Define empty macro for lock API
    #define BD_LOCK_TYPE     /**/
    #define INIT_BD_LOCK     /**/
    #define BD_LOCK(x)       /**/
    #define BD_UNLOCK(x)     /**/
#endif


#endif
