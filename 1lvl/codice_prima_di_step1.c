#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/wait.h>

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

typedef struct _node{
    unsigned long mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned long mem_size;
    unsigned long  val; //per i bit etc;
    unsigned pos; //posizione all'interno dell'array "tree"
    
} node;

unsigned master;

node* tree; //array che rappresenta l'albero, tree[0] è dummy! l'albero inizia a tree[1]
unsigned long overall_memory_size;
unsigned number_of_nodes; //questo non tiene presente che tree[0] è dummy! se qua c'è scritto 7 vuol dire che ci sono 7 nodi UTILIZZABILI
void* overall_memory;
node* trying;
unsigned failed_at_node;
unsigned overall_height;
unsigned mypid;
node* upper_bound;

int number_of_processes;


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


unsigned long upper_power_of_two(unsigned long v);
void init_node(node* n);
void init_tree(unsigned long number_of_nodes);
void init(unsigned long memory);
void free_nodes(node* n); //questo fa la free fisicamente
void end();
void print(node* n);
bool alloc(node* n);

bool check_parent(node* n);
void smarca_(node* n);

void smarca(node* n){
    upper_bound = &ROOT;
    smarca_(n);
}

void print_in_profondita(node*);
void print_in_ampiezza();
void free_node_(node* n);

/*Queste funzioni sono esposte all'utente*/
node* request_memory(unsigned pages);
void free_node(node* n){
    upper_bound = &ROOT;
    free_node_(n);
}

unsigned log2_(unsigned long value);

//MARK: WRITE SU FILE

/*
 SCRIVE SU FILE I NODI PRESI DA UN THREAD - FUNZIONE PRETTAMENTE DI DEBUG
 */
void write_taken(){
    char filename[128];
    sprintf(filename, "./debug/taken_%d.txt", getpid());
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



/*
 SCRIVE SU FILE LA SITUAZIONE DELL'ALBERO (IN AMPIEZZA) VISTA DA UN CERTO THREAD
 */
void write_on_a_file_in_ampiezza(){
    char filename[128];
    sprintf(filename, "./debug/tree.txt");
    FILE *f = fopen(filename, "w");
    int i;
    
    if (f == NULL){
        printf("Error opening file!\n");
        exit(1);
    }
    
    for(i=1;i<=number_of_nodes;i++){
        node* n = &tree[i];
        fprintf(f, "(%p) %u val=%lu has %lu B. mem_start in %lu  level is %u\n", (void*)n, tree[i].pos,  tree[i].val , tree[i].mem_size, tree[i].mem_start,  level(n));
    }
    
    fclose(f);
}


//MARK: MATHS
/*
 CALCOLA LA POTENZA DI DUE
 */


//Random limitato da limit [0,limit].. estremi inclusi
unsigned rand_lim(unsigned limit) {
    /* return a random number between 0 and limit inclusive.
     */
    int divisor = RAND_MAX/(limit+1);
    int retval;
    
    do {
        retval = rand() / divisor;
    } while (retval > limit);
    
    
    return retval;
}

unsigned long upper_power_of_two(unsigned long v){
    v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; v++;
    return v;
}


/*log2 malato*/
const unsigned tab64[64] = {
    63,  0, 58,  1, 59, 47, 53,  2,
    60, 39, 48, 27, 54, 33, 42,  3,
    61, 51, 37, 40, 49, 18, 28, 20,
    55, 30, 34, 11, 43, 14, 22,  4,
    62, 57, 46, 52, 38, 26, 32, 41,
    50, 36, 17, 19, 29, 10, 13, 21,
    56, 45, 25, 31, 35, 16,  9, 12,
    44, 24, 15,  8, 23,  7,  6,  5};

unsigned log2_ (unsigned long value){
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return tab64[((unsigned long)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}

//MARK: INIT


/*
 Questa funzione inizializza l'albero. Non il nodo dummy (tree[0])
 @param number_of_nodes: the number of nodes.
 */
void init_tree(unsigned long number_of_nodes){
    int i=0;


    ROOT.mem_start = 0;
    ROOT.mem_size = overall_memory_size;
    ROOT.pos = 1;
    ROOT.val = 0;

    
    //init_node(ROOT);
    for(i=2;i<=number_of_nodes;i++){
        
        tree[i].pos = i;
        node parent = parent(&tree[i]);
        tree[i].val = 0;
        tree[i].mem_size = parent.mem_size / 2;
        
        if(left_index(&parent)==i)
            tree[i].mem_start = parent.mem_start;
        
        else
            tree[i].mem_start = parent.mem_start + tree[i].mem_size;
        
    }
}

/*
 @param pages: pagine richieste. Questo sarà il valore della radice
 */
void init(unsigned long levels){
    
    number_of_nodes = (1<<levels) -1;
    
    overall_height = levels;
    
    unsigned number_of_leaves = (1<< (levels-1));
    
    overall_memory_size = MIN_ALLOCABLE_BYTES * number_of_leaves;
    
    overall_memory = mmap(NULL, overall_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    if(overall_memory==MAP_FAILED)
        abort();
    
    
    tree = mmap(NULL,(1+number_of_nodes)*sizeof(node), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    if(tree==MAP_FAILED)
        abort();
    
    init_tree(number_of_nodes);
    
    puts("init complete");
    
}

//MARK: FINE

/*
 Funzione ricorsiva. Chiama se stessa su sui figli e tornando indietro effettua la free (di sistema) sul nodo n
 @param n: il nodo da deallocare (a livello sistema)
 */
/*void free_nodes(node* n){
    if(left_index(n)<= number_of_nodes){ //right != NULL <=> left != NULL
        free_nodes(left(n));
        free_nodes(right(n));
    }
    free(n);
}*/

/*
 Funzione finale che nell'ordine:
 1) libera la memoria assegnabile
 2) invoca la free_nodes sulla radice
 3) libera l'array che memorizzava l'albero
 */
void end(){
    free(overall_memory);
    //free_nodes(ROOT);
    free(tree);
}

//MARK: PRINT


/*traversal tramite left and right*/

void print_in_profondita(node* n){
    //printf("%u has\n", n->pos);
    printf("%u has %lu B. mem_start in %lu left is %u right is %u level=%u\n", n->pos, n->mem_size, n->mem_start, left_index(n), right_index(n), level(n));
    if(left_index(n)<= number_of_nodes){
        print_in_profondita(&left(n));
        print_in_profondita(&
                            right(n));
    }
}

/*Print in ampiezza*/

void print_in_ampiezza(){
    
    int i;
    for(i=1;i<=number_of_nodes;i++){
        //printf("%p\n", tree[i]);
        printf("%u has %lu B. mem_start in %lu val is %lu level=%u\n", tree[i].pos, tree[i].mem_size,tree[i].mem_start, tree[i].
               val, level(&tree[i]));
        //printf("%u has %lu B\n", tree[i]->pos, tree[i]->mem_size);
    }
}


//MARK: ALLOCAZIONE
/*
 Questa funzione si preoccupa di marcare il bit relativo al sottoalbero del blocco che ho allocato in tutti i suoi antenati.
 Questa funziona ritorna true se e solo se riesce a marcare fino alla radice. La funzione fallisce se e solo se si imbatte in un nodo occupato
 (riconoscibile dal value che contiene 0X10). La funzione si preoccupa inoltre di cancellare l'eventuale bit di coalescing.
 Side effect: se fallisce la variabile globale failed_at_node assume il valore del nodo dove la ricorsione è fallita
 @param n: nodo da cui iniziare la risalita (che è già marcato totalmente o parzialmente)
 @return true se è tutto corretto fino alla radice, false altrimenti
 
 */
bool check_parent(node* n){
    
    node* actual = &parent(n);
    
    unsigned long actual_value;
    unsigned long new_value;
    
    do{
        actual_value = actual->val;
        
        //Se l'AND con OCCUPY fallisce vuol dire che qualcuno lo ha occupato
        if((actual_value & OCCUPY)!=0){
            failed_at_node = actual->pos;
            
            //ripristino dal nodo dove sono partito al nodo dove sono arrivato (da trying ad n)
            upper_bound = n;
            free_node_(trying);
            
            return false;
        }
        
        new_value = actual_value;
        
        if(&left(actual) == n){ //n è sinistro
            new_value = new_value & MASK_CLEAN_LEFT_COALESCE;
            new_value = new_value | MASK_OCCUPY_LEFT;
        }
        
        else{
            new_value = new_value & MASK_CLEAN_RIGHT_COALESCE;
            new_value = new_value | MASK_OCCUPY_RIGHT;
        }
        
    }while(!__sync_bool_compare_and_swap(&actual->val, actual_value, new_value));
    
    if(actual==&ROOT)
        return true;
    
    return check_parent(actual);
}


/*
 Prova ad allocare un DATO nodo.
 Side effect: la variabile globale trying assume il valore del nodo n che qui è passato come parametro
 Side effect: se fallisce subito, prima di chiamare la check_parent la variabile globale failed_at_node assumerà il valore n
 @param n: nodo presunto libero (potrebbe essere diventato occupato concorrentemente)
 @return true se l'allocazione riesce, false altrimenti
 
 */
bool alloc(node* n){
    unsigned long actual;
    //actual è il valore dei bit che sono nel nodo prima che ci lavoro
    actual = n->val;
    trying = n;
    
    //Il nodo è già occupato (parzialmente o totalmente)
    if(actual != 0){
        failed_at_node = n->pos;
        return false;
    }
    
    //il nodo è stato parallelamente occupato. Parzialmente o totalmente
    if(!__sync_bool_compare_and_swap(&n->val,actual,OCCUPY_BLOCK)){
        failed_at_node = n->pos;
        return false;
    }
    
    //ho allocato tutto l'albero oppure sono riuscito a risalire fino alla radice
    if(n==&ROOT || check_parent(n)){
        return true;
    }
    
    else{
        return false;
    }
}



/*
 Funzione di malloc richiesta dall'utente.
 @param pages: memoria richiesta dall'utente
 @return l'indirizzo di memoria allocato per la richiesta; NULL in caso di fallimento
 
 
 */
node* request_memory(unsigned byte){
    if(byte>MAX_ALLOCABLE_BYTE)
        return NULL;
    
    byte = upper_power_of_two(byte);
    if(byte<MIN_ALLOCABLE_BYTES)
        byte = MIN_ALLOCABLE_BYTES;
    unsigned starting_node = overall_memory_size / byte; //first node for this level
    unsigned last_node = left_index(&tree[starting_node])-1; //last node for this level
    
    //actual è il posto in cui iniziare a cercare
    
    unsigned actual = mypid % number_of_processes;
    
    if(last_node-starting_node!=0)
        actual = actual % (last_node - starting_node);
    else
        actual=0;
    
    actual = starting_node + actual;
    
    
    unsigned started_at = actual;
    
    bool restarted = false;
    
    //quando faccio un giro intero ritorno NULL
    do{
        if(alloc(&tree[actual])==true){
            return &tree[actual];
        }
        
        
        if(failed_at_node==1){ // il buddy è pieno
            return NULL;
        }
        
        //Questo serve per evitare tutto il sottoalbero in cui ho fallito
        actual=(failed_at_node+1)* (1<<( level(&tree[actual]) - level(& tree[failed_at_node])));
        
        
        if(actual>last_node){ //se ho sforato riparto dal primo utile, se il primo era quello da cui avevo iniziato esco al controllo del while
            actual=starting_node;
            restarted = true;
        }
        
    }while(restarted==false || actual < started_at);
    
    return NULL;
}

//MARK: FREE

/*
 Questa funzione libera il nodo n e si preoccupa di settare il bit di coalesce per tutti gli antenati del nodo (fino all'upper bound).
 La funzione chiamata smarca() si preoccuperà di impostare i bit di utilizzo a 0 per gli antenati, ove appropriato.
 @param n: nodo da liberare
 @param upper_bound: l'ultimo nodo da liberare
 */
void free_node_(node* n){
    if(n->val!=OCCUPY_BLOCK){
        
        printf("err: il blocco non è occupato\n");
        return;
    }
    
    node* actual = &parent(n);
    node* runner = n;
    unsigned long actual_value;
    unsigned long new_value;
    
    while(runner!=upper_bound){
        do{
            actual_value = actual->val;
            if(&left(actual)==runner)
                new_value = actual_value | MASK_LEFT_COALESCE;
            else
                new_value = actual_value | MASK_RIGHT_COALESCE;
        }while(!__sync_bool_compare_and_swap(&actual->val,actual_value, new_value));
        runner = actual;
        actual = &
        
        parent(actual);
    }
    
    n->val = 0; //controlla se ci vuole la CAS
    //print_in_ampiezza();
    if(n!=upper_bound)
        smarca_(n);
}


/*
 Questa funziona leva il bit di coalesce e libera il sottoramo (fino all'upper bound) ove necessario. Nota che potrebbe terminare lasciando alcuni bit di coalescing posti ad 1. Per i dettagli si guardi la documentazione.
 @param n: il figlio del primo nodo da smarcare! (BISOGNA SMARCARE DAL PADRE)
 @param upper_bound; l'ultimo nodo da smarcare
 */
void smarca_(node* n){
    
    node* actual = &
    parent(n);
    unsigned long actual_value;
    unsigned long new_val;
    do{
        actual_value = actual->val;
        new_val = actual_value;
        //libero il rispettivo sottoramo su new val
        if(&left(actual)==n && (actual_value & MASK_LEFT_COALESCE)==0 ){ //if n è sinistro AND b1=0...già riallocato
            return;
        }
        else if(&right(actual)==n && (actual_value & MASK_RIGHT_COALESCE)==0){ //if n è destro AND b1=0...già riallocato
            return;
        }
        if (&left(actual)==n)
            new_val = new_val & MASK_CLEAN_LEFT_COALESCE & MASK_CLEAN_OCCUPIED_LEFT;
        else
            new_val = new_val & MASK_CLEAN_RIGHT_COALESCE & MASK_CLEAN_OCCUPIED_RIGHT;
    } while (!__sync_bool_compare_and_swap(&actual->val,actual_value,new_val));
    if(actual==upper_bound) //se sono arrivato alla radice ho finito
        return;
    if(&left(actual)==n && (actual->val & MASK_OCCUPY_RIGHT)!=0) //if n è sinistro AND (parent(n).actual_value.b4=1) Interrompo! Mio nonno deve vedere il sottoramo occupato perchè mio fratello tiene occupato mio padre!!
        return;
    else if(&
            right(actual)==n && (actual->val & MASK_OCCUPY_LEFT)!=0) // if n è destro AND (parent(n).actual_value.b3=2)
        return;
    else
        smarca_(actual);
    
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
    int i=0;
    int tentativi = 10000000 /number_of_processes ;
    int allocazioni;
    int free;
    free=allocazioni=0;
    for(i=0;i<tentativi;i++){
        
        //stampo ogni tanto per controllare che il sistema non si è bloccato
        //if(i%100000==0)
        //    printf("(%u) %d\n", mypid, i);
        
        unsigned long scelta = rand();
        
        //FAI L'ALLOCAZIONE
        if(scelta>=((RAND_MAX/10)*5)){ // 50% di probabilità fai la malloc
            
            //QUA CON SCELTA VIENE DECISO IL NUMERO DELLE PAGINE DA ALLOCARE
            scelta = rand_lim(log2_(MAX_ALLOCABLE_BYTE/MIN_ALLOCABLE_BYTES));
            scelta = (MIN_ALLOCABLE_BYTES) << (scelta);
            if(scelta==0)
                scelta=1;
            
            node* obt;
            if(takenn_serbatoio->number==0){
                printf("Allocazioni %d free %d allocazioni-free=%d\n", allocazioni, free, allocazioni-free);
                exit(0);
            }
            
            obt = request_memory(scelta);
            
            if (obt==NULL){
                //printf("not enough memory\n");
                failures[write_failures_on]++;
                continue;
            }
            
            allocazioni++;
            //printf("giro %d\n",i);
            taken_list_elem* t = takenn_serbatoio->head;
            takenn_serbatoio->head = takenn_serbatoio->head->next;
            takenn_serbatoio->number--;
            t->elem = obt;
            t->next = NULL;
            if(takenn->head!=NULL)
                t->next = takenn->head;
            takenn->head = t;
            takenn->number++;
        }
        
        else{//FAI UNA FREE A CASO
            
            if(takenn->number==0){
                continue;
            }
            free++;

            //scelgo il nodo da liberare (nella mia taken list
            scelta = rand_lim((takenn->number)-1);
            
            if(scelta==0){
                free_node(takenn->head->elem);
                taken_list_elem* die = takenn->head;
                takenn->head = takenn->head->next;
                takenn->number--;
                die->next = takenn_serbatoio->head;
                takenn_serbatoio->head = die;
                takenn_serbatoio->number++;
            }
            else{
                taken_list_elem* runner = takenn->head;
                taken_list_elem* chosen;
                int j=0;
                
                for(j=0;j<scelta-1;j++)
                    runner = runner->next;
                
                chosen = runner->next;
                free_node(chosen->elem);
                
                if(runner->next!=NULL)
                    runner->next = runner->next->next;
                else
                    runner->next=NULL;
                
                takenn->number--;
                chosen->next = takenn_serbatoio->head;
                takenn_serbatoio->head = chosen;
                takenn_serbatoio->number++;
            }
            
        }
        
    }
    
    
}

int main(int argc, char**argv){
    
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

