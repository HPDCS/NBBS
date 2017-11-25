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
#include "timer.h"

//#define MYDEBUG
#define FAIL_END 12
#define ITER 465
#define SERBATOIO_DIM (16*8192)

static taken_list* takenn; //__thread taken_list* takenn
static taken_list* takenn_serbatoio; //__thread taken_list* takenn_serbatoio

unsigned int number_of_processes;
//unsigned int master;
unsigned int mypid;
unsigned int pcount = 0;
__thread unsigned int myid; //__thread unsigned int myid=0;

static unsigned long long *volatile failures, *volatile allocs, *volatile frees, *volatile ops;
static nbint *volatile memory;
unsigned int *start;

void * do_alloca(unsigned long long size) __attribute__((used));
void * do_alloca(unsigned long long size){
	void * addr;
	taken_list_elem *t;
	
	if(takenn_serbatoio->number==0){
		printf("Allocazioni %llu free %llu allocazioni-free=%llu\n", allocs[myid], frees[myid], allocs[myid] - frees[myid]);
		exit(0);
	}
	
	addr = request_memory(size);
	if (addr==NULL){
		failures[myid]++;
		return NULL;
	}

	allocs[myid]++;
	memory[myid]+=size;
	
	//estraggo un nodo dal serbatoio
	t = takenn_serbatoio->head;
	takenn_serbatoio->head = takenn_serbatoio->head->next;
	takenn_serbatoio->number--;
	//inserisco il nodo tra quelli presi
	t->elem = addr;
	t->elem->mem_size = size;
	t->next = takenn->head;;
	takenn->head = t;
	takenn->number++;
	
	return addr;
}

void libera(unsigned long long scelta) __attribute__((used));
void libera(unsigned long long scelta){
	taken_list_elem *runner, *chosen;

	if(scelta == 0){
		scelta = rand_lim((takenn->number)-1);
	}
	frees[myid]++;
	
	if(scelta==0){
		free_node(takenn->head->elem);
		scelta = (takenn->head->elem->mem_size);
		chosen = takenn->head;
		takenn->head = takenn->head->next;
	}
	else{
		runner = takenn->head;
		for(unsigned int j=0;j<scelta-1;j++)
			runner = runner->next;
		
		chosen = runner->next;
		free_node(chosen->elem);
		scelta = (chosen->elem->mem_size);
		runner->next = runner->next->next;
	}
	memory[myid]-=scelta;
	takenn->number--;
	chosen->next = takenn_serbatoio->head;
	takenn_serbatoio->head = chosen;
	takenn_serbatoio->number++;
}


void parallel_try(){
	unsigned int i, j, tentativi;
	unsigned long long scelta;
	unsigned int scelta_lvl;
	unsigned int tmp = 0;
	
	void *obt;
	//taken_list_elem *t, *runner, *chosen;
	
	scelta_lvl = log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES);
	tentativi = ops[myid] = 10000000 / number_of_processes ;
	i = j = 0;


	srand(17*myid);
	
	int count = 0;
	for(i=0;i<tentativi;i++){

		scelta = rand();
		//ALLOC
		if(scelta >=((RAND_MAX/10)*5)){ 		//if(scelta > ((RAND_MAX/10)*takenn->number)){

			scelta = (MIN_ALLOCABLE_BYTES) << (rand_lim(scelta_lvl)); //<<rimpiazza con un exp^(-1)
			if(scelta==0)
				scelta = MIN_ALLOCABLE_BYTES;
			scelta = upper_power_of_two(scelta);

			obt = do_alloca(scelta);
		}
		//FREE
		else{
			if(takenn->number==0){
				i--;
				continue;
			}
			libera(0);
		}
	}
}

void init_run(){
	unsigned int j;
	taken_list_elem* runner;
	
	//child code, do work and exit.
	myid = getpid() % number_of_processes;// __sync_fetch_and_add(myid, 1);//

	takenn = malloc(sizeof(taken_list));
	takenn->head = NULL;
	takenn->number = 0;
	
	takenn_serbatoio = malloc(sizeof(taken_list));
	takenn_serbatoio->number = SERBATOIO_DIM;
	takenn_serbatoio->head = malloc(sizeof(taken_list_elem));
	runner = takenn_serbatoio->head;
	
	for(j=0;j<SERBATOIO_DIM;j++){
		runner->next = malloc(sizeof(taken_list_elem));
		runner = runner->next;
	}
	
	
	while(*start==0);
	
	parallel_try();
	
	free(takenn);
	exit(0); //pthread_exit(NULL);
}


__attribute__((constructor(400))) void pre_main2(int argc, char**argv){
	number_of_processes=atoi(argv[1]);
	failures = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	allocs = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	frees = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	ops = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	memory = mmap(NULL, sizeof(nbint) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	start = mmap(NULL, sizeof(unsigned int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*start = 0;
	for(unsigned int i=0; i<number_of_processes; i++){
		allocs[i] = frees[i] = failures[i] = ops[i] = memory[i] = 0;
	}
}


int main(int argc, char**argv){
	int status, local_pid, i=0;
	unsigned long long exec_time;
	unsigned long long total_fail = 0, total_alloc = 0, total_free = 0, total_ops = 0;
	nbint total_mem = 0;
	
	srand(17);
	
	if(argc!=2){
		printf("usage: ./a.out <number of threads>\n");
		exit(0);
	}
	number_of_processes=atoi(argv[1]);
	
	//pthread_t p_tid[number_of_processes];    
	for(i=0; i<number_of_processes; i++){
		//if( (pthread_create(&p_tid[i], NULL, init_run, NULL)) != 0) {
        //    fprintf(stderr, "%s\n", strerror(errno));
        //    abort();
        //}
		
		if(fork()==0){
			init_run(i);
		}
	}
	clock_timer_start(exec_time);
	__sync_fetch_and_add(start,1);
	
	
	//for(i = 0; i < number_of_processes; i++){
	//	pthread_join(p_tid[i], NULL);
	//}
	//only parent reach this. Wait all processes (-1 parameters) and terminate
	while ( (local_pid = waitpid(-1, &status, 0)) ){
		if(!WIFEXITED(status)) printf("Figlio %d uscito per errore %d\n", local_pid, WTERMSIG(status));
		if (errno == ECHILD) break;
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
