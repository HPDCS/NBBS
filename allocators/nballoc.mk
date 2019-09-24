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

ifdef NUM_LEVELS
FLAGS :=$(FLAGS) -DNUM_LEVELS=$(NUM_LEVELS)ULL
endif


ifdef NUM_LEVELS
FLAGS :=$(FLAGS) -DNUM_LEVELS=$(NUM_LEVELS)ULL
endif

TARGET = $(notdir $(shell pwd))

OBJS := nballoc.o

-include $(OBJS:.o=.d)

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(FLAGS) $*.c -o $*.o
	ld -r $*.o ../../utils/utils.o -o nballoc-$(TARGET).o
	ar rcs lib$(TARGET).a nballoc-$(TARGET).o


	
clean:
	rm *.o *.d *.a

.PHONY: clean