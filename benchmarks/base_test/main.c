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

//#define MYDEBUG
#define FAIL_END 12
#define ITER 465
#define SERBATOIO_DIM (16*8192)

static taken_list* takenn;
static taken_list* takenn_serbatoio;

unsigned int number_of_processes;
unsigned int master;
unsigned int pcount = 0;
unsigned int mypid;
__thread unsigned int myid;

static unsigned long long *volatile failures, *volatile allocs, *volatile frees, *volatile ops;
static nbint *volatile memory;


void parallel_try(){
	unsigned int i, j, tentativi;
	unsigned long scelta;
	unsigned int scelta_lvl;
	unsigned int tmp = 0;
	
	node* obt;
	taken_list_elem *t, *runner, *chosen;
	
	scelta_lvl = log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES);
	tentativi = ops[myid] = 10000000 / number_of_processes ;
	i = j = 0;

	srand(17);
	
	//printf("[%u] pid=%u tentativi=%u ops=%llu\n", myid, mypid, tentativi, ops[myid]);
	int count = 0;
	for(i=0;i<tentativi;i++){

			#ifdef MYDEBUG
				
				if( i == ITER)
					exit(1);
			#endif
		scelta = rand();
		//ALLOC
		if(scelta >=((RAND_MAX/10)*5)){
		//if(scelta > ((RAND_MAX/10)*takenn->number)){
			
			if(allocs[myid]-frees[myid] >= 2048) tmp++;

			//QUA CON SCELTA VIENE DECISO IL NUMERO DELLE PAGINE DA ALLOCARE
			scelta = (MIN_ALLOCABLE_BYTES) << (rand_lim(scelta_lvl)); //<<rimpiazza con un exp^(-1)
			if(scelta==0)
				scelta=1;
			scelta = upper_power_of_two(scelta);
			
			if(takenn_serbatoio->number==0){
				printf("Allocazioni %llu free %llu allocazioni-free=%llu\n", allocs[myid], frees[myid], allocs[myid] - frees[myid]);
				exit(0);
			}


			#ifdef MYDEBUG
				//printf("INIZIO MALLOC\n");
				#endif
			obt = request_memory(scelta);
			if (obt==NULL){
			#ifdef MYDEBUG
				printf("%s scelta:%d iter:%d alloc:%d frees:%d\n", "FAILURE", scelta, i, allocs[myid], frees[myid] );
				#endif
				failures[myid]++;
			#ifdef MYDEBUG
				
				if(count++ == FAIL_END || i == ITER)
					exit(1);
			#endif
				continue;
			}
			#ifdef MYDEBUG
			else
				printf("MALLOC %s scelta:%d pointer: %p iter:%d alloc:%d frees:%d\n", "OK", scelta, obt,  i, allocs[myid], frees[myid] );
			#endif


			//printf("%s scelta:%d pointer %p iter:%d alloc:%d frees:%d\n", "MALLOC", scelta, obt, i, allocs[myid], frees[myid] );

			allocs[myid]++;
			memory[myid]+=scelta;
			
			//estraggo un nodo dal serbatoio
			t = takenn_serbatoio->head;
			takenn_serbatoio->head = takenn_serbatoio->head->next;
			takenn_serbatoio->number--;
			//inserisco il nodo tra quelli presi
			t->elem = obt;
			t->elem->mem_size = scelta;
			t->next = takenn->head;;
			takenn->head = t;
			takenn->number++;
		}
		//FREE
		else{
			if(takenn->number==0){
				//printf("WTF\n");
				i--; //le allocazioni non fatte non contano...
				continue;
			}
			frees[myid]++;

			//scelgo il nodo da liberare nella mia taken list
			scelta = rand_lim((takenn->number)-1); //ritorna un numero da 0<head> a number-1<ultimo>


			
			if(scelta==0){


			#ifdef MYDEBUG
				printf("%s scelta:%d pointer %p iter:%d alloc:%d frees:%d\n", "FREE", takenn->head->elem->mem_size, takenn->head->elem, i, allocs[myid], frees[myid] );
			#endif
				free_node(takenn->head->elem);
			#ifdef MYDEBUG
				//printf("FINE FREE\n");
			#endif
				scelta = (takenn->head->elem->mem_size);
				chosen = takenn->head;
				takenn->head = takenn->head->next;
			}
			else{
				runner = takenn->head;
				for(j=0;j<scelta-1;j++)
					runner = runner->next;
				
				chosen = runner->next;
			#ifdef MYDEBUG
				printf("%s scelta:%d pointer %p iter:%d alloc:%d frees:%d\n", "FREE", chosen->elem->mem_size, chosen->elem, i, allocs[myid], frees[myid] );
			#endif
				free_node(chosen->elem);
			#ifdef MYDEBUG
				//printf("FINE FREE\n");
			#endif
				scelta = (chosen->elem->mem_size);
				runner->next = runner->next->next;
			}
			memory[myid]-=scelta;
			takenn->number--;
			chosen->next = takenn_serbatoio->head;
			takenn_serbatoio->head = chosen;
			takenn_serbatoio->number++;
		}
		//write_on_a_file_in_ampiezza_start(i);
	}
	//__asm__ __volatile__ ("mfence" ::: "memory");
	//printf("[%u] FINI:%u %llu\n", myid, i, ops[myid]);
	//printf("%s %d\n", "FAIL ALLOC PER ", tmp);
	 
}


__attribute__((constructor(400))) void pre_main2(int argc, char**argv)
{

	number_of_processes=atoi(argv[1]);
	failures = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	allocs = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	frees = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	ops = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	memory = mmap(NULL, sizeof(nbint) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}


int main(int argc, char**argv){
	srand(17);
	int status, local_pid;
	
	if(argc!=2){
		printf("usage: ./a.out <number of threads> <number of levels>\n");
		exit(0);
	}
	
	//Per scrivere solo una volta il risultato finale su file
	master=getpid();
	int i=0;

	number_of_processes=atoi(argv[1]);
	//unsigned long requested = atol(argv[2]);
	

	for(i=0; i<number_of_processes; i++){
		allocs[i] = 0;
		frees[i] = 0;
		failures[i] = 0;
		ops[i] = 0;
		memory[i] = 0;
	}
	
	for(i=0; i<number_of_processes; i++)
	{
		if(fork()==0)
		{
			//child code, do work and exit.
			mypid = getpid();//__sync_fetch_and_add(id_counter, 1);//
			myid = mypid % number_of_processes;
			//printf("ID: %d\n", myid);			
			
			//write_failures_on = mypid % number_of_processes;
			//per la gestione dei nodi presi dal singolo processo.
			takenn = malloc(sizeof(taken_list));
			takenn->head = NULL;
			takenn->number = 0;
			
			takenn_serbatoio = malloc(sizeof(taken_list));
			takenn_serbatoio->number = SERBATOIO_DIM;
			takenn_serbatoio->head = malloc(sizeof(taken_list_elem));
			taken_list_elem* runner = takenn_serbatoio->head;
			int j;
			for(j=0;j<SERBATOIO_DIM;j++){
				runner->next = malloc(sizeof(taken_list_elem));
				runner = runner->next;
			}
			
			parallel_try();
			
			free(takenn);
			exit(0);
		}
		//else parent spawn processes
	}
	
	//only parent reach this. Wait all processes (-1 parameters) and terminate
	while ( (local_pid = waitpid(-1, &status, 0)) ){
	if(!WIFEXITED(status)) printf("Figlio %d uscito per errore %d\n", local_pid, WTERMSIG(status));
	//printf("Il figlio %d è terminato\n", local_pid);
		if (errno == ECHILD) {
			break;
		}
	}
	   
	//__asm__ __volatile__ ("mfence" ::: "memory");
	unsigned long long total_fail = 0, total_alloc = 0, total_free = 0, total_ops = 0;
	nbint total_mem = 0;
		
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
