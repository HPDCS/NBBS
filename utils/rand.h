#if KERNEL_BD == 1
# define __glibc_unlikely(cond)	__builtin_expect ((cond), 0)
#endif

struct my_drand48_data
  {
    unsigned short int _x[3];        /* Current state.  */
    unsigned short int _old_x[3]; /* Old state.  */
    unsigned short int _c;        /* Additive const. in congruential formula.  */
    unsigned short int _init;        /* Flag for initializing.  */
    __extension__ unsigned long long int _a;        /* Factor in congruential
                                                   formula.  */
  };

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;

static inline int
_my_drand48_iterate (unsigned short int xsubi[3], struct my_drand48_data *buffer)
{
  uint64_t X;
  uint64_t result;
  /* Initialize buffer, if not yet done.  */
  if (__glibc_unlikely (!buffer->_init))
    {
      buffer->_a = 0x5deece66dull;
      buffer->_c = 0xb;
      buffer->_init = 1;
    }
  /* Do the real work.  We choose a data type which contains at least
     48 bits.  Because we compute the modulus it does not care how
     many bits really are computed.  */
  X = (uint64_t) xsubi[2] << 32 | (uint32_t) xsubi[1] << 16 | xsubi[0];
  result = X * buffer->_a + buffer->_c;
  xsubi[0] = result & 0xffff;
  xsubi[1] = (result >> 16) & 0xffff;
  xsubi[2] = (result >> 32) & 0xffff;
  return 0;
}


static inline int
_my_nrand48_r (unsigned short int xsubi[3], struct my_drand48_data *buffer,
             long int *result)
{
  /* Compute next state.  */
  if (_my_drand48_iterate (xsubi, buffer) < 0)
    return -1;
  /* Store the result.  */
  if (sizeof (unsigned short int) == 2)
    *result = xsubi[2] << 15 | xsubi[1] >> 1;
  else
    *result = xsubi[2] >> 1;
  return 0;
}

static inline int
my_lrand48_r (struct my_drand48_data *buffer, long int *result)
{
  /* Be generous for the arguments, detect some errors.  */
  if (buffer == NULL)
   return -1;
  return _my_nrand48_r (buffer->_x, buffer, result);
}


static inline int
my_srand48_r (long int seedval, struct my_drand48_data *buffer)
{
  /* The standards say we only have 32 bits.  */
  if (sizeof (long int) > 4)
    seedval &= 0xffffffffl;
  buffer->_x[2] = seedval >> 16;
  buffer->_x[1] = seedval & 0xffffl;
  buffer->_x[0] = 0x330e;
  buffer->_a = 0x5deece66dull;
  buffer->_c = 0xb;
  buffer->_init = 1;
  return 0;
}
