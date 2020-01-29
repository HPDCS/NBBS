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
* 
* 
* This file implements an allocator that access directly the Linux Kernel Buddy system.
* 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <numaif.h>
#include "nballoc.h"
#include "utils.h"

#define PAGE_SIZE (4096)
#define NUM_PAGES (128)
#define NUM_EXTRACTION (NUM_PAGES) //NUM_EXTRACTIONS must not exceed NUM_PAGES unless you would like to segfault

//#define EMPTY_ZERO

#ifndef EMPTY_ZERO
char buff[PAGE_SIZE] = {[0 ... (PAGE_SIZE-1)] = 0x0f};
#else
char *buff;
#endif

#define AUDIT if(0)
#define CYCLES 20000000

#define NUM_THREADS 10

unsigned int number_of_processes;

int allocate(int order, unsigned long * temp){
	if(syscall(134,order,temp) == -1)
		*temp = NULL;
}

int deallocate(unsigned long address, int order){
	return syscall(174,address,order);
}



void* bd_xx_malloc(size_t byte){
    unsigned int starting_node, last_node, actual, started_at, failed_at_node;
    bool restarted = false;
    unsigned int leaf_position;
    unsigned long address;
        
    if(tid == -1){
		tid = __sync_fetch_and_add(&partecipants, 1);
     }


    if( byte > MAX_ALLOCABLE_BYTE || byte > overall_memory_size)
        return NULL;	

    byte = upper_power_of_two(byte);

    if( byte < MIN_ALLOCABLE_BYTES )
        byte = MIN_ALLOCABLE_BYTES;

    allocate(log_2(byte/MIN_ALLOCABLE_BYTES), &address);

    return (void*) address;
}



