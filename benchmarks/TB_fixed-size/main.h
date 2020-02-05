#include "parameters.h"

#if KERNEL_BD == 0
#include <rand.h>
#define ALLOC_GET_PAR(x,y) x
#define  FREE_GET_PAR(x,y) x
#else
#include "../../utils/rand.h"
unsigned long ids = 0;
#define ALLOC_GET_PAR(x,y) y
#define  FREE_GET_PAR(x,y) x,y
#endif

void* bd_xx_malloc(size_t);
void  bd_xx_free(FREE_GET_PAR(void*,size_t));


void fixedsize(ALLOC_GET_PAR(unsigned long long fixed_size, unsigned int fixed_order), unsigned int number_of_processes, unsigned long long *allocs, unsigned long long *failures, unsigned long long *frees){
	unsigned int i, j, k, tentativi = CO_ITERATIONS / number_of_processes;
	unsigned long r;
	unsigned long long blocchi = (1<<CO_LEVELS) - 1;
	unsigned long long taglie = CO_LEVELS;
	
#if KERNEL_BD == 0
	void *obt, *cmp = NULL;
	void **chunks = malloc(sizeof(void*)*blocchi);
	unsigned long long *sizes = malloc(sizeof(unsigned long long)*blocchi);
	unsigned long long max_size = fixed_size << (CO_LEVELS-1);	
	unsigned long long scelta;
#else
	unsigned long myid = __sync_fetch_and_add(&ids, 1);
	unsigned long long cmp = 0ULL;
	unsigned long long *chunks = vmalloc(sizeof(void*)*blocchi);
	unsigned int *sizes  	   = vmalloc(sizeof(unsigned int)*blocchi);
	unsigned int max_order = fixed_order + (CO_LEVELS-1);
	unsigned int scelta;
#endif
	struct my_drand48_data randBuffer;
    my_srand48_r(17*myid, &randBuffer);
	
	i = j = k = 0;
	
	for(i=0; i < taglie; i++){
#if KERNEL_BD == 0
		scelta = max_size>>i;
#else
		scelta = max_order-i;
#endif
		for(j=0; j<(1<<i); j++){
			sizes[k] = scelta;
			chunks[k++] = TO_BE_REPLACED_MALLOC(scelta);
		}
	}
	 
	for(i=0;i<tentativi;i++){
		my_lrand48_r(&randBuffer, &r);
		j = (unsigned int)r;
		j = j % blocchi;
		TO_BE_REPLACED_FREE(FREE_GET_PAR(chunks[j], sizes[j]));
		chunks[j] = TO_BE_REPLACED_MALLOC(sizes[j]);
		if (chunks[j] == cmp){
			(*failures)++;
			continue;
		}
		(*allocs)++;
		(*frees)++;
	}	
#if KERNEL_BD == 0
	free(chunks);
	free(sizes);
#else
	vfree(chunks);
	vfree(sizes);
#endif
}
