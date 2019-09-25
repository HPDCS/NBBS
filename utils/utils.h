#ifndef __NB_ALLOC_UTILS__
#define __NB_ALLOC_UTILS__

unsigned int rand_lim(unsigned int limit);
unsigned int upper_power_of_two(unsigned int v);
unsigned int log2_ (unsigned long value);

#define ENABLE_CACHE 1

extern __thread unsigned int freemap[];

static inline void update_freemap(unsigned int key, unsigned int value){
    unsigned int tmp = 
  #if ENABLE_CACHE == 1
    freemap[key];
  #else
    0;
  #endif
    if(1 || tmp == 0 || value < tmp) 
freemap[key] = value;
}

static inline unsigned int get_freemap(unsigned int key, unsigned int max){
  #if ENABLE_CACHE == 0
     return 0;
  #endif
     unsigned int tmp = freemap[key];
     freemap[key] += freemap[key] != 0;
     freemap[key] = (-(freemap[key]<max)) & freemap[key];
     return tmp;
}
#endif
