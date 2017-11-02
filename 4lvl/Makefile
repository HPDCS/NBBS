.PHONY: clean

FLAGS = -O2 -Wall

clean:
	touch ./debug/safe.txt; rm ./debug/*.txt ; make compile

compile: codice_multilivello.c
	gcc $(FLAGS)  codice_multilivello.c   -o codice;
	
standard_parallel_run: codice_multilivello.c
	gcc $(FLAGS)  ./codice_multilivello.c -o codice;
	time ./codice $(proc) $(levels) ; 
	
parallel_run: codice_multilivello.c
	gcc $(FLAGS)  ./codice_multilivello.c -o codice -D MAX_ALLOCABLE_BYTE=$(MAX) -D MIN_ALLOCABLE_BYTES=$(MIN);
	time ./codice $(proc) $(levels) ; 

numa: codice_multilivello.c
	gcc $(FLAGS)   ./codice_multilivello.c -o codice -lnuma -D NUMA -D MAX_ALLOCABLE_BYTE=$(MAX) -D MIN_ALLOCABLE_BYTES=$(MIN);
	time ./codice $(proc) $(levels) ; 
	
standard_numa: codice_multilivello.c
	gcc -$(FLAGS)   ./codice_multilivello.c -o codice -lnuma -D NUMA;
	time ./codice $(proc) $(levels) ; 
