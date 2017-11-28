
/**************************************************************************
   this user program allows you to access the sys_log_message 
   and sys_get_message system calls added to the kernel via the
   sys-call-table-hacker module

   NUM_PAGES represents the virtual addressing span of the buffer
   used as destination for sys_get_messge in case you compile
   with the EMPTY_ZERO macro

   NUM_EXTRACTIONS defines the number of iterations for calling the 
   sys_get_message system-call - in case you run with EMPTY_ZERO it
   must not exceed NUM_PAGES (since you are displacing page by page into 
   the destination buffer while iterations go through)

   if EMPTY_ZERO is not active then the buffer is of fixed size PAGE_SIZE
   and is located on the .data section given that it is initialized

   please try changing NUM_PAGES (and NUM_EXRACTIONS) when ZERO_EMPTY is active
   and compare performance and actual run time (e.g. errors) vs the configuration 
   with no EMPTY_ZERO

   VERY IMPORTANT: argv[2] (the system-call number for the actual servoice to be invocked)
   nust be retrived vi dmesg after mounting the sys-call-table-hacker module
**************************************************************************/

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



