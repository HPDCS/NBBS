all: kernel 1lvl 4lvl


kernel: 
	gcc  user-allocate-test.c -lnuma -lpthread -o kernel

1lvl: 
	gcc  user-allocate-test-nb1.c nballoc1.c utils.c -lnuma -lpthread -o 1lvl


4lvl: 
	gcc  user-allocate-test-nb4.c nballoc4.c utils.c -lnuma -lpthread -o 4lvl

clean:
	rm kernel 1lvl 4lvl