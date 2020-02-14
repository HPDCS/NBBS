#if KERNEL_BD == 0
#define ALLOC_GET_PAR(x,y) x
#define  FREE_GET_PAR(x,y) x
#define BASE 4096
#else
#define ALLOC_GET_PAR(x,y) y
#define  FREE_GET_PAR(x,y) x,y
#define BASE 1
#endif

void* bd_xx_malloc(size_t);
void  bd_xx_free(FREE_GET_PAR(void*,size_t));

#include "parameters.h"


void threadtest(ALLOC_GET_PAR(unsigned long long fixed_size, unsigned int fixed_order), unsigned int number_of_processes, unsigned long long *allocs, unsigned long long *failures, unsigned long long *frees){
	unsigned int i, j, tentativi = TT_OBJS / number_of_processes;
	unsigned int iterations = TT_ITERATIONS;
#if KERNEL_BD == 0
	void *obt, *cmp = NULL;
	void **addrs = malloc(sizeof(void*)*tentativi);
	tentativi /= fixed_size/BASE;
	iterations *= fixed_size/BASE;
#else
	unsigned long long cmp = 0ULL;
	unsigned long long *addrs = vmalloc(sizeof(void*)*tentativi);
	tentativi /= BASE >> fixed_order;
	tentativi *= BASE >> fixed_order;
#endif

	i = j = 0;

	for(j=0; j<iterations; j++){
		for(i=0;i<tentativi;i++){
			addrs[i] = TO_BE_REPLACED_MALLOC(ALLOC_GET_PAR(fixed_size, fixed_order));
			if (addrs[i] == cmp){
				(*failures)++;
			}
			else
				(*allocs)++;
		}
		
		for(i=0;i<tentativi;i++){
			if(addrs[i] != cmp){
				TO_BE_REPLACED_FREE(FREE_GET_PAR(addrs[i], fixed_order));
				(*frees)++;
			}
		}
	}
#if KERNEL_BD == 0
	free(addrs);
#else
	vfree(addrs);
#endif
}
