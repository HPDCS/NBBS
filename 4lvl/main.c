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

#ifdef NUMA
    #include <numa.h>
    #include <numaif.h>
    #include <sched.h>
#endif

int number_of_processes;
#ifdef NUMA
int number_of_numa_nodes;
#endif

taken_list* takenn;
taken_list* takenn_serbatoio;
int* failures;
int write_failures_on;

/*
    SCRIVE SU FILE I NODI PRESI DA UN THREAD - FUNZIONE PRETTAMENTE DI DEBUG
 */
void write_taken(){
    char filename[128];
    sprintf(filename, "./debug/taken_%d.txt", getpid()%number_of_processes);
    FILE *f = fopen(filename, "w");
    unsigned i;
    
    if (f == NULL){
        printf("Error opening file!\n");
        exit(1);
    }
    
    taken_list_elem* runner = takenn->head;
    
    /* print some text */
    for(i=0;i<takenn->number;i++){
        fprintf(f, "%u\n", runner->elem->pos);
        runner=runner->next;
    }
    
    
    
    fclose(f);
    
}

//MARK: ESECUZIONE

#ifdef SERIAL

void try(){
    unsigned long scelta;
    node* result = NULL;
    
    while(1){
        puts("scrivi 1 per alloc, 2 per free");
        scanf("%lu", &scelta);
        switch(scelta){
            case 1:
                printf("inserisci le pagine che vuoi allocare (MAX %lu)\n", overall_memory_pages);
                scanf("%lu", &scelta);
                result = request_memory(scelta);
                if(result==NULL)
                    puts("allocazione fallita");
                break;
            case 2:
                printf("inserisci l'indice del blocco che vuoi liberare\n"); //quesot non dovrà essere cosi ma stiamo debuggando.. in realtà la free deve essere chiamata senza interazione con l'utente
                scanf("%lu", &scelta);
                free_node(&tree[(int) scelta]);
                break;
            default:
                continue;
    
        }
        puts("Dopo l'esecuzione, l'albero, in ampiezza è:");
        print_in_ampiezza();
    }
}

int main(int argc, char**argv){

    
    puts("main single thread");
    
    if(argc!=2){
        printf("usage: ./a.out <requested memory (in pagine)>\n");
        exit(0);
    }
    number_of_processes = 1;
    unsigned long requested = atol(argv[1]);
    init(requested);
    
    try();
    
    end();
    
    return 0;
}

#else

/*
 
    Con questa funzione faccio un po' di free e di malloc a caso.
 
 */
void parallel_try(){
    int i, j, allocazioni, free, tentativi;
    unsigned long scelta;
    unsigned int scelta_lvl;
    
    node* obt;
    taken_list_elem *t, *runner, *chosen;
    
    scelta_lvl = log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES);
    tentativi = 10000000 / number_of_processes ;
    i = j = free = allocazioni = 0;
    
    for(i=0;i<tentativi;i++){
		
        scelta = rand();
        
        //ALLOC
        //if(scelta >=((RAND_MAX/10)*5)){
        if(scelta >= ((RAND_MAX/10)*takenn->number)){
            
            //QUA CON SCELTA VIENE DECISO IL NUMERO DELLE PAGINE DA ALLOCARE
            scelta = (MIN_ALLOCABLE_BYTES) << (rand_lim(scelta_lvl)); //<<rimpiazza con un exp^(-1)
            //if(scelta==0)
                scelta=1;
            
            if(takenn_serbatoio->number==0){
                printf("Allocazioni %d free %d allocazioni-free=%d\n", allocazioni, free, allocazioni-free);
                exit(0);
            }
            
            obt = request_memory(scelta);
            if (obt==NULL){
                failures[write_failures_on]++;
                continue;
            }
            allocazioni++;
            
            //estraggo un nodo dal serbatoio
            t = takenn_serbatoio->head;
            takenn_serbatoio->head = takenn_serbatoio->head->next;
            takenn_serbatoio->number--;
            //inserisco il nodo tra quelli presi
            t->elem = obt;
            t->next = takenn->head;;
            takenn->head = t;
            takenn->number++;
        }
        //FREE
        else{
            if(takenn->number==0){
                continue;
            }
            free++;

            //scelgo il nodo da liberare nella mia taken list
            scelta = rand_lim((takenn->number)-1); //ritorna un numero da 0<head> a number-1<ultimo>
            
            if(scelta==0){
                free_node(takenn->head->elem);
                chosen = takenn->head;
                takenn->head = takenn->head->next;
            }
            else{
				runner = takenn->head;
                for(j=0;j<scelta-1;j++)
                    runner = runner->next;
                
                chosen = runner->next;
                free_node(chosen->elem);
                runner->next = runner->next->next;
            }
            takenn->number--;
            chosen->next = takenn_serbatoio->head;
            takenn_serbatoio->head = chosen;
            takenn_serbatoio->number++;
        }
    } 
}

int main(int argc, char**argv){
    //time_t time1;
    //time_t time2;
    //time ( &time1 );
    if(argc!=3){
        printf("usage: ./a.out <number of threads> <number of levels>\n");
        exit(0);
    }
    
#ifdef NUMA
    if(numa_available()<0){
        puts("numa is not available");
        exit(-1);
    }
    number_of_numa_nodes = numa_max_node() + 1;
    //printf("numa nodes are %d\n", number_of_numa_nodes);
    //printf("%d\n", numa_available());
#endif
    
    //Per scrivere solo una volta il risultato finale su file
    master=getpid();
    int i=0;

    number_of_processes=atoi(argv[1]);
    unsigned long requested = atol(argv[2]);
    
    init(requested);
    failures = mmap(NULL, sizeof(int) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    //time ( &time2 );
    //printf("time1= %lu\n", time1);
    //printf("time2= %lu\n", time2);
    //printf("time2-time1=%lu\n", time2 -time1);
    for(i=0; i<number_of_processes; i++){
        failures[i] = 0;
        if(fork()==0){
            //child code, do work and exit.
            mypid=getpid();
            write_failures_on = mypid % number_of_processes;
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
    while (waitpid(-1, NULL, 0)) {
        if (errno == ECHILD) {
            break;
        }
    }
    puts("failures:");
    int total = 0;;
    for(i=0;i<number_of_processes;i++){
        printf("%d: %d\n", i, failures[i]);
        total += failures[i];
    }
    printf("total failure is %d\n", total);
   //write_on_a_file_in_ampiezza();
    
    return 0;
}

#endif
