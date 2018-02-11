#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <string.h>
#include "utils.h"
#include "timer.h"

//#define MYDEBUG
#define FAIL_END 12
#define ITER 465
#define SERBATOIO_DIM (16*8192)


#ifndef ALLOC_SIZE
#define ALLOC_SIZE 8
#endif

void* allocate(int order);
void deallocate(void* address, int order);

//__thread taken_list* takenn;
//__thread taken_list* takenn_serbatoio;

__thread void *addrs[100000];

unsigned int number_of_processes;
//unsigned int master;
unsigned int pcount = 0;
__thread unsigned int myid=0;

static unsigned long long *volatile failures, *volatile allocs, *volatile frees, *volatile ops;
static unsigned long long *volatile memory;
unsigned int *start;

size_t min_size;
size_t max_size;
size_t gap_size;
unsigned int taglie;
unsigned int blocchi;
void *** chunks;
size_t ** sizes;

void parallel_try(){
	unsigned int i, j, k, tentativi;
	size_t scelta;
	unsigned int tmp = 0;
	
	
	void *obt;
	//taken_list_elem *t, *runner, *chosen;
	
	//scelta_lvl = log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES);
	tentativi = ops[myid] = 20000000 / number_of_processes ;
	i = j = k = 0;

	srand(17*myid);
		
	for(i=0; i< taglie; i++){
		scelta = max_size>>i;
		for(j=0; j<(1<<i); j++){
			sizes[myid][k] = scelta;
			chunks[myid][k++] = TO_BE_REPLACED_MALLOC(scelta);
		}
	}
	
	for(i=0;i<tentativi;i++){
		j = rand() % blocchi;
		TO_BE_REPLACED_FREE(chunks[myid][j],sizes[myid][j]);
		chunks[myid][j] = TO_BE_REPLACED_MALLOC(sizes[myid][j]);
		if (chunks[myid][j]==NULL){
			failures[myid]++;
			continue;
		}
		allocs[myid]++;
		frees[myid]++;
	}
}

void * init_run(){
	unsigned int j;
	
	//child code, do work and exit.
	myid = __sync_fetch_and_add(&pcount, 1);//myid = getpid() % number_of_processes;// 
		
	while(*start==0);
	
	parallel_try();
	
	pthread_exit(NULL);
}


__attribute__((constructor(400))) void pre_main2(int argc, char**argv){
	unsigned int i;
	unsigned int tmp=0, min_lvl=0, max_lvl=0;

	if(argc!=4){
		printf("usage: ./a.out <number of threads>\n");
		exit(0);
	}
	number_of_processes = atoi(argv[1]);
	min_size = tmp = atoll(argv[2]);
	while(tmp != 0){
		min_lvl++;
		tmp = tmp >> 1;
	}
	max_size = tmp = atoll(argv[3]);
	while(tmp != 0){
		max_lvl++;
		tmp = tmp >> 1;
	}
	gap_size = (max_size - min_size)/4096 + 1;
	taglie = max_lvl - min_lvl + 1;
	blocchi = (1<<taglie) - 1;
	
	
	number_of_processes=atoi(argv[1]);
	failures = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	allocs = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	frees = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	ops = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	memory = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	start = mmap(NULL, sizeof(unsigned int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	chunks = mmap(NULL, sizeof(void*)*number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	sizes = mmap(NULL, sizeof(void*)*number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	*start = 0;
	for(i=0; i<number_of_processes; i++){
		allocs[i] = frees[i] = failures[i] = ops[i] = memory[i] = 0;
		chunks[i] = mmap(NULL, sizeof(void*)*blocchi, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		sizes[i] = mmap(NULL, sizeof(size_t)*blocchi, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	}
}


int main(int argc, char**argv){
  printf("USING ALLOCATOR: %s\n", ALLOCATOR_NAME);
	int status, local_pid, i=0;
	unsigned long long exec_time;
	unsigned long long total_fail = 0, total_alloc = 0, total_free = 0, total_ops = 0;
	unsigned long long total_mem = 0;
	
	srand(17);
	
	printf("Avvio test a taglia costante da %lu a %lu con %u taglie differenti e %u blocchi\n", min_size, max_size, taglie, blocchi);
	
	
	pthread_t p_tid[number_of_processes];    
	for(i=0; i<number_of_processes; i++){
		if( (pthread_create(&p_tid[i], NULL, init_run, NULL)) != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            abort();
        }		
	}
	clock_timer_start(exec_time);
	__sync_fetch_and_add(start,1);
	
	
	for(i = 0; i < number_of_processes; i++){
		pthread_join(p_tid[i], NULL);
	}
	
	printf("Timer  (clocks): %llu\n",clock_timer_value(exec_time));
	
	   
		
	printf("_______________________________________\n");
		printf("tot_ops expected: %10llu\n",  ops[0]);
		
	for(i=0;i<number_of_processes;i++){
		//if(i>0) printf(". . . . . . . . . . . . . . . \n");
		printf("[%d]: TOT_OPS      %10llu: ",i, allocs[i]+frees[i]+failures[i]);
		printf("\t allocati: %10llu ;", allocs[i]);
		printf("\t dealloca: %10llu ;", frees[i]);
		printf("\t failures: %10llu ;", failures[i]);
		printf("\t memory  : %10llu Bytes \n", memory[i]);
		total_fail += failures[i];
		total_alloc += allocs[i];
		total_free += frees[i];
		total_ops += ops[i];
		total_mem += memory[i];
	}
	printf("_______________________________________\n");
	printf("Total ops exp     %10llu\n", total_ops);
	printf("total ops done:   %10llu\n", total_alloc + total_free + total_fail);
	printf("total allocs:     %10llu\n", total_alloc);
	printf("total frees:  	  %10llu\n", total_free);
	printf("       diff:  	  %10llu\n", total_alloc-total_free);
	printf("        mem:  	  %10llu Bytes\n", total_mem);
	printf("............................\n");	
	printf("total failures:   %10llu\n", total_fail);
#ifdef DEBUG
	printf("total nodes alloc:%10llu\n", *node_allocated);
	printf("total memo alloc: %10llu Bytes\n", *size_allocated);
	//write_on_a_file_in_ampiezza();
#endif
	
	return 0;
}
