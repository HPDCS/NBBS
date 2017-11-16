CC=gcc
CFLAGS=-c -O3 -g -Wall -MMD -MP -MF $*.d

OBJS := utils.o

-include $(OBJS:.o=.d)

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $*.c -o $*.o
	
clean:
	rm *.o *.d 

.PHONY: clean