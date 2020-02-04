#include <stdlib.h>
#include "utils.h"

__thread unsigned int freemap[128];

unsigned int rand_lim(unsigned int limit) {
    /* return a random number between 0 and limit inclusive.
     */
    int divisor = RAND_MAX/(limit+1);
    int retval;
    
    do {
        retval = rand() / divisor;
    } while (retval > limit);
    
    
    return retval;
}
