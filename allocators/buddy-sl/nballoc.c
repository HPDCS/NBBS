#include "nballoc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define NODE_UNUSED 0
#define NODE_USED 1	
#define NODE_SPLIT 2
#define NODE_FULL 3

#ifndef NUM_LEVELS
#define NUM_LEVELS 20
#endif

#ifndef MIN_ALLOCABLE_BYTES
#define MIN_ALLOCABLE_BYTES 8ULL //(2KB) numero minimo di byte allocabili
#endif
#ifndef MAX_ALLOCABLE_BYTE
#define MAX_ALLOCABLE_BYTE  16384ULL //(16KB)
#endif

#define BD_SPIN_LOCK 1

#if BD_SPIN_LOCK == 1
	#define BD_LOCK_TYPE pthread_spinlock_t 
	#define INIT_BD_LOCK pthread_spin_init
	#define BD_LOCK   	 pthread_spin_lock
	#define BD_UNLOCK 	 pthread_spin_unlock
#else
	#define BD_LOCK_TYPE pthread_mutex_t 
	#define INIT_BD_LOCK pthread_mutex_init
	#define BD_LOCK   	 pthread_mutex_lock
	#define BD_UNLOCK 	 pthread_mutex_unlock
#endif

struct buddy {
	BD_LOCK_TYPE lock;
	long level;
	uint8_t tree[1];//<-------------------------------------------------------------
};

struct buddy *self;

void* overall_memory;

BD_LOCK_TYPE lock;

__attribute__((constructor(500))) void buddy_new() {
	long size = 1 << (NUM_LEVELS-1);
	self = malloc(sizeof(struct buddy) + sizeof(uint8_t) * (size * 2 - 2));//<------------------------------------------
	overall_memory = malloc(size*MIN_ALLOCABLE_BYTES);

	self->level = NUM_LEVELS-1;
	memset(self->tree , NODE_UNUSED , size*2-1);
	//return self;
	
	#if BD_SPIN_LOCK == 0
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(self->lock), &attr);
    #else
    pthread_spin_init(&(self->lock), PTHREAD_PROCESS_SHARED);
    #endif
	
	printf("Buddy init mem address: %p", overall_memory);
	
}

void
buddy_delete() {
	free(self);
}

static inline long
is_pow_of_2(unsigned long long x) {
	return !(x & (x-1));
}

static inline unsigned long
next_pow_of_2(unsigned long x) {
	if ( is_pow_of_2(x) )
		return x;
	x |= x>>1;
	x |= x>>2;
	x |= x>>4;
	x |= x>>8;
	x |= x>>16;
	x |= x>>32;
	return x+1;
}

static inline long
_index_offset(long index, long level, long max_level) {
	return ((index + 1) - (1 << level)) << (max_level - level);
}

static void 
_mark_parent(long index) {
	for (;;) {
		long buddy = index - 1 + (index & 1) * 2;
		if (buddy > 0 && (self->tree[buddy] == NODE_USED ||	self->tree[buddy] == NODE_FULL)) {
			index = (index + 1) / 2 - 1;
			self->tree[index] = NODE_FULL;
		} else {
			return;
		}
	}
}

void*
bd_bd_malloc(size_t s) {
	long size = (long) s;
	if (size == 0) {
		size = 1;
	}
	size = (s-1)/8 + 1;
	
	size = (long)next_pow_of_2(size);
	
	long length = 1 << self->level;

	if (size > length)
		return NULL;

	long index = 0;
	long level = 0;

	while (index >= 0) {
		if (size == length) {
			if (self->tree[index] == NODE_UNUSED) {
				self->tree[index] = NODE_USED;
				_mark_parent(index);
				return ((char*)overall_memory) + _index_offset(index, level, self->level)*MIN_ALLOCABLE_BYTES;
			}
		} else {
			// size < length
			switch (self->tree[index]) {
			case NODE_USED:
			case NODE_FULL:
				break;
			case NODE_UNUSED:
				// split first
				self->tree[index] = NODE_SPLIT;
				self->tree[index*2+1] = NODE_UNUSED;
				self->tree[index*2+2] = NODE_UNUSED;
			default:
				index = index * 2 + 1;
				length /= 2;
				level++;
				continue;
			}
		}
		if (index & 1) {
			++index;
			continue;
		}
		for (;;) {
			level--;
			length *= 2;
			index = (index+1)/2 -1;
			if (index < 0)
				return NULL;
			if (index & 1) {
				++index;
				break;
			}
		}
	}
	
	return NULL;
}

void* bd_xx_malloc(size_t s){
	void* res;
	BD_LOCK(&(self->lock));
	res = bd_bd_malloc(s);
	BD_UNLOCK(&(self->lock));
	return res;
}

static void 
_combine(long index) {
	for (;;) {
		long buddy = index - 1 + (index & 1) * 2;
		if (buddy < 0 || self->tree[buddy] != NODE_UNUSED) {
			self->tree[index] = NODE_UNUSED;
			while (((index = (index + 1) / 2 - 1) >= 0) &&  self->tree[index] == NODE_FULL){
				self->tree[index] = NODE_SPLIT;
			}
			return;
		}
		index = (index + 1) / 2 - 1;
	}
}

void
bd_bd_free(void *addr) {
	//char* tmp  = addr - overall_memory;
	long offset = ((unsigned long long)(addr-overall_memory))/MIN_ALLOCABLE_BYTES;
	
	assert( offset < (1<< self->level));
	long left = 0;
	long length = 1 << self->level;
	long index = 0;

	
	for (;;) {
		switch (self->tree[index]) {
		case NODE_USED:
			assert(offset == left);
			_combine(index);
			return;
		case NODE_UNUSED:
			assert(0);
			return;
		default:
			length /= 2;
			if (offset < left + length) {
				index = index * 2 + 1;
			} else {
				left += length;
				index = index * 2 + 2;
			}
			break;
		}
	}
		
}


void bd_xx_free(void* a){
	BD_LOCK(&self->lock);
	bd_bd_free(a);
	BD_UNLOCK(&self->lock);
}

int
buddy_size(long offset) {
	assert( offset < (1<< self->level));
	long left = 0;
	long length = 1 << self->level;
	long index = 0;

	for (;;) {
		switch (self->tree[index]) {
		case NODE_USED:
			assert(offset == left);
			return length;
		case NODE_UNUSED:
			assert(0);
			return length;
		default:
			length /= 2;
			if (offset < left + length) {
				index = index * 2 + 1;
			} else {
				left += length;
				index = index * 2 + 2;
			}
			break;
		}
	}
}

static void
_dump(long index , long level) {
	switch (self->tree[index]) {
	case NODE_UNUSED:
		printf("(%ld:%d)", _index_offset(index, level, self->level) , 1 << (self->level - level));
		break;
	case NODE_USED:
		printf("[%ld:%d]", _index_offset(index, level, self->level) , 1 << (self->level - level));
		break;
	case NODE_FULL:
		printf("{");
		_dump(index * 2 + 1 , level+1);
		_dump(index * 2 + 2 , level+1);
		printf("}");
		break;
	default:
		printf("(");
		_dump(index * 2 + 1 , level+1);
		_dump(index * 2 + 2 , level+1);
		printf(")");
		break;
	}
}

void
buddy_dump() {
	_dump(0 , 0);
	printf("\n");
}

