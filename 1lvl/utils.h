#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/wait.h>

unsigned rand_lim(unsigned limit);
unsigned long upper_power_of_two(unsigned long v);
unsigned log2_ (unsigned long value);
