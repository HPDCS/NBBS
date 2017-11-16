CC=gcc
CFLAGS=-c -O3 -g -Wall -I../../utils -MMD -MP -MF $*.d

ifdef DEBUG
FLAGS :=$(FLAGS) -DDEBUG
endif

ifdef MIN
FLAGS:= $(FLAGS) -DMIN_ALLOCABLE_BYTES=$(MIN)ULL
endif

ifdef MAX
FLAGS:= $(FLAGS) -DMAX_ALLOCABLE_BYTE=$(MAX)ULL
endif

OBJS := nballoc.o

-include $(OBJS:.o=.d)

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(FLAGS) $*.c -o $*.o
	
clean:
	rm *.o *.d 

.PHONY: clean