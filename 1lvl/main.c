#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/wait.h>
#include "nballoc.h"
#include "utils.h"


int number_of_processes;
unsigned master;
unsigned mypid;

taken_list* takenn;
taken_list* takenn_serbatoio;
unsigned long long *failures, *allocs, *frees;
int write_failures_on;


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
                break;
            case 2:
                printf("inserisci l'indice del blocco che vuoi liberare\n"); //quesot non dovrà essere cosi ma stiamo debuggando.. in realtà la free deve essere chiamata senza interazione con l'utente
                scanf("%lu", &scelta);
                free_node(tree[(int) scelta]);
                break;
            default:
                continue;
                
        }
        if(result==NULL)
            puts("allocazione fallita");
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
    int i, j, tentativi;
    unsigned long scelta;
    unsigned int scelta_lvl;
    
    node* obt;
    taken_list_elem *t, *runner, *chosen;
    
    scelta_lvl = log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES);
    tentativi = 10000000 / number_of_processes ;
    i = j = 0;
    
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
                printf("Allocazioni %llu free %llu allocazioni-free=%llu\n", allocs[write_failures_on], frees[write_failures_on], allocs[write_failures_on] - frees[write_failures_on]);
                exit(0);
            }
            
            obt = request_memory(scelta);
            if (obt==NULL){
                failures[write_failures_on]++;
                continue;
            }
            allocs[write_failures_on]++;
            
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
            frees[write_failures_on]++;

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
    printf("Test in concorrenza\n");
    if(argc!=3){
        printf("usage: ./a.out <number of threads> <number of levels>\n");
        exit(0);
    }
    
    //Per scrivere solo una volta il risultato finale su file
    master=getpid();
    int i=0;
    
    number_of_processes=atoi(argv[1]);
    unsigned long requested = atol(argv[2]);
    
    
    init(requested);
    failures = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    allocs = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    frees = mmap(NULL, sizeof(unsigned long long) * number_of_processes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    for(i=0; i<number_of_processes; i++){
        failures[i] = 0;
        if(fork()==0){
            //child code, do work and exit.
            mypid=getpid();
            
            write_failures_on = mypid % number_of_processes; //<---NON VA BENE
            //per la gestione dei nodi presi dal singolo processo.
            takenn = malloc(sizeof(taken_list));
            takenn->head = NULL;
            takenn->number = 0;
            
            //crea lista di nodi da riusare 
            takenn_serbatoio = malloc(sizeof(taken_list));
            takenn_serbatoio->number = SERBATOIO_DIM;
            takenn_serbatoio->head = malloc(sizeof(taken_list_elem));
            taken_list_elem* runner = takenn_serbatoio->head;
            int j;
            for(j=0;j<SERBATOIO_DIM;j++){
                runner->next = malloc(sizeof(taken_list_elem));
                runner = runner->next;
            }
            runner->next = NULL;
            
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
    
    int total = 0;;
    for(i=0;i<number_of_processes;i++){
        printf("Process %d:\n",i);
        printf("\t allocati: %llu\n", allocs[i]);
        printf("\t dealloca: %llu\n", frees[i]);
        printf("\t failures: %llu\n", failures[i]);
        total += failures[i];
    }
    printf("total failure is %d\n", total);
    //write_on_a_file_in_ampiezza();
    
    return 0;
}

#endif

