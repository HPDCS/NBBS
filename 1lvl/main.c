#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/wait.h>
#include "nballoc.h"

//bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval)

#define OCCUPY ((unsigned long) (0x10))
#define MASK_CLEAN_LEFT_COALESCE ((unsigned long)(~(MASK_LEFT_COALESCE)))
#define MASK_CLEAN_RIGHT_COALESCE ((unsigned long)(~(MASK_RIGHT_COALESCE)))
#define MASK_OCCUPY_RIGHT ((unsigned long) (0x1))
#define MASK_OCCUPY_LEFT ((unsigned long) (0x2))
#define MASK_LEFT_COALESCE ((unsigned long) (0x8))
#define MASK_RIGHT_COALESCE ((unsigned long) (0x4))
//PARAMETRIZZAZIONE
#ifndef MIN_ALLOCABLE_BYTES
#define MIN_ALLOCABLE_BYTES 8 //(2KB) numero minimo di byte allocabili
#endif
#ifndef MAX_ALLOCABLE_BYTE
#define MAX_ALLOCABLE_BYTE  16384 //(16KB)
#endif
#define FREE_BLOCK ((unsigned long) 0)
#define OCCUPY_BLOCK ((OCCUPY) | (MASK_OCCUPY_LEFT) | (MASK_OCCUPY_RIGHT))
#define MASK_CLEAN_OCCUPIED_LEFT (~(MASK_OCCUPY_LEFT))
#define MASK_CLEAN_OCCUPIED_RIGHT (~(MASK_OCCUPY_RIGHT))

#define ROOT (tree[1])
#define left(n) (tree[((n->pos)*(2))])
#define right(n) (tree[(((n->pos)*(2))+(1))])
#define left_index(n) (((n)->pos)*(2))
#define right_index(n) ((((n)->pos)*(2))+(1))
#define parent(n) (tree[(unsigned)(((n)->pos)/(2))])
#define parent_index(n) ((unsigned)(((n)->pos)/(2)))
#define level(n) ((unsigned) ( (overall_height) - (log2_(( (n)->mem_size) / (MIN_ALLOCABLE_BYTES )) )))
#define SERBATOIO_DIM (16*8192)

#define PAGE_SIZE (4096)

int number_of_processes;
unsigned master;
unsigned mypid;


typedef struct _taken_list_elem{
    struct _taken_list_elem* next;
    node* elem;
}taken_list_elem;

typedef struct _taken_list{
    struct _taken_list_elem* head;
    unsigned number;
}taken_list;


taken_list* takenn;
taken_list* takenn_serbatoio;
int* failures;
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
    int i, j, allocazioni, free, tentativi;
    unsigned long scelta;
    
    node* obt;
    taken_list_elem *t, *runner, *chosen;
    
    tentativi = 10000000 /number_of_processes ;
    i=j=free=allocazioni=0;
    
    for(i=0;i<tentativi;i++){
		
        scelta = rand();
        
        //FAI L'ALLOCAZIONE
        //if(scelta >=((RAND_MAX/10)*5) && takenn->number < 10){ // 50% di probabilità fai la malloc
        if(scelta >= ((RAND_MAX/10)*takenn->number)){ // 50% di probabilità fai la malloc
            
            //QUA CON SCELTA VIENE DECISO IL NUMERO DELLE PAGINE DA ALLOCARE
            scelta = rand_lim(log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES));
            scelta = (MIN_ALLOCABLE_BYTES) << (scelta);
          //  if(scelta==0)
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
        
        else{//FAI UNA FREE A CASO
            
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
                /*
                if(runner->next!=NULL)
                    runner->next = runner->next->next;
                else
                    runner->next=NULL;
                */
                
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

