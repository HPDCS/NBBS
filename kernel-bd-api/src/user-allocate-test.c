
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
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/mman.h>
#include <numaif.h>

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
	return syscall(134,order,temp);
}

int deallocate(unsigned long address, int order){
	return syscall(174,address,order);
}

int order;

pthread_t tid[NUM_THREADS];

void allocator(void* res){
       // printf("started cross checker\n");
	int i;
	unsigned long address;
	int ret;

	for (i=0; i< CYCLES/number_of_processes; i++){
		ret = allocate(order,&address);
//		printf("the address is %p\n",address);
		if(ret == 0) deallocate(address,order);
	}
}



int main(int argc, char** argv){
	
	unsigned long address;
	int i;
	int ret;
	unsigned long nodemask = 0x01;

	if(argc < 3){
		printf("usage: prog order\n");
		return 1;
	}
	
	set_mempolicy(MPOL_BIND, &nodemask,sizeof(unsigned long));

	order = strtol(argv[1],NULL,10);
	number_of_processes = atoi(argv[2]);

	for (i=0; i < NUM_THREADS; i++){
		ret = pthread_create(&tid[i],NULL,allocator,NULL);
	}

	for (i=0; i < NUM_THREADS; i++){
		ret = pthread_join(tid[i],NULL);
	}
	
/*
	for (i=0; i< CYCLES; i++){
		ret = allocate(order,&address);
		printf("the address is %p\n",address);
		if(ret == 0) deallocate(address,order);
	}
*/

}
