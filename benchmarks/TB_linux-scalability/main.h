#if KERNEL_BD == 0
#define ALLOC_GET_PAR(x,y) x
#define  FREE_GET_PAR(x,y) x
#else
#define ALLOC_GET_PAR(x,y) y
#define  FREE_GET_PAR(x,y) x,y
#endif

void* bd_xx_malloc(size_t);
void  bd_xx_free(FREE_GET_PAR(void*,size_t));

#include "parameters.h"

void linux_scalability(ALLOC_GET_PAR(unsigned long long fixed_size, unsigned int fixed_order), unsigned long long *allocs, unsigned long long *failures, unsigned long long *frees){
	unsigned int i, tentativi;
	#if KERNEL_BD == 0
	void *obt, *cmp = NULL;
	#else
	unsigned long long obt, cmp = 0ULL;
	#endif
	
	tentativi = LS_ITERATIONS; // / number_of_processes ;
	i = 0;

	for(i=0;i<tentativi;i++){
		obt = TO_BE_REPLACED_MALLOC(ALLOC_GET_PAR(fixed_size, fixed_order));
		if (obt==cmp){
			(*failures)++;
			continue;
		}

		(*allocs)++;
		TO_BE_REPLACED_FREE(FREE_GET_PAR(obt, fixed_order));
		(*frees)++;
	}
}
