#ifndef __NB_ALLOC_UTILS__
#define __NB_ALLOC_UTILS__

unsigned int rand_lim(unsigned int limit);
//unsigned long upper_power_of_two(unsigned long v);
//unsigned int log2_ (unsigned long value);
//int convert_to_level(unsigned long long size);

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

#define PAGE_SIZE (4096)



static inline unsigned long upper_power_of_two(unsigned long v){
    v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; v++;
    return v;
}


/*log2 malato*/
static const unsigned int tab64[64] = {
    63,  0, 58,  1, 59, 47, 53,  2,
    60, 39, 48, 27, 54, 33, 42,  3,
    61, 51, 37, 40, 49, 18, 28, 20,
    55, 30, 34, 11, 43, 14, 22,  4,
    62, 57, 46, 52, 38, 26, 32, 41,
    50, 36, 17, 19, 29, 10, 13, 21,
    56, 45, 25, 31, 35, 16,  9, 12,
    44, 24, 15,  8, 23,  7,  6,  5};

static inline unsigned int log2_ (unsigned long value){
    return 63 - __builtin_clzl(value);
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return tab64[((unsigned long)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}



static inline int convert_to_level(unsigned long long size){
	unsigned long long tmp = (size - 1)/PAGE_SIZE + 1;
	return (int) log2_(tmp);	
}


#endif
