//PARAMETRIZZAZIONE
#ifndef MIN_ALLOCABLE_BYTES
#define MIN_ALLOCABLE_BYTES 8 //(2KB) numero minimo di byte allocabili
#endif
#ifndef MAX_ALLOCABLE_BYTE
#define MAX_ALLOCABLE_BYTE  16384 //(16KB)
#endif

#define SERBATOIO_DIM (16*8192)

#define PAGE_SIZE (4096)

typedef struct _node{
    unsigned long mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned long mem_size;
    unsigned long  val; //per i bit etc;
    unsigned pos; //posizione all'interno dell'array "tree"
} node;

extern unsigned mypid;

void init(unsigned long memory);
void free_nodes(node* n); //questo fa la free fisicamente
void free_node(node* n);
node* request_memory(unsigned pages);
