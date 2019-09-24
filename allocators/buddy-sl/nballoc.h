#ifndef BUDDY_MEMORY_ALLOCATION_H
#define BUDDY_MEMORY_ALLOCATION_H


#include <unistd.h>

void *bd_xx_alloc(size_t size);
void bd_xx_free(void *addrs);

#endif
