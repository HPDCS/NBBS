CC=gcc
FLAGS=-g -Wall -pthread -lm
#INCLUDE= -I include/ -I mm/ -I core/ -Istatistics/
EXECUTABLE=test

all: test

test: main.o nballoc.o
	$(CC) $(FLAGS) nballoc.o main.o -o $(EXECUTABLE)
	
main.o : main.c
	$(CC) $(FLAGS) main.c -o main.o -c

register.o : nballoc.c nballoc.h timer.h	
	$(CC) $(FLAGS) nballoc.c -o nballoc.o -c
	
clean:
	rm *.o test
