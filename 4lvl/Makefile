CC=gcc
FLAGS=-g -Wall -lm
#INCLUDE= -I include/ -I mm/ -I core/ -Istatistics/
EXECUTABLE=test

all: test

test: main.o nballoc.o utils.o
	$(CC) $(FLAGS) nballoc.o main.o utils.o -o $(EXECUTABLE)
	
main.o : main.c
	$(CC) $(FLAGS) main.c -o main.o -c

register.o : nballoc.c nballoc.h timer.h	
	$(CC) $(FLAGS) nballoc.c -o nballoc.o -c
	
utils.o : utils.c utils.h
	$(CC) $(FLAGS) utils.c -o utils.o -c
	
clean:
	rm *.o test
