include ../Makefile.inc

#TARGET = $(TEST)-ptmalloc3 $(TEST)-libc $(TEST)-hoard 
TARGET = $(TEST)-ptmalloc3 $(TEST)-libc 
ALLOCATORS_ABS_PATH = $(abspath ../../allocators)

all: $(TARGET)  

$(TEST)-libc: $(TEST).c
	$(CC) $(CCFLAGS) $(TEST).c -o $@ -lpthread -lm

$(TEST)-hoard: $(TEST).c
	$(CC) $(CCFLAGS) $(TEST).c -o $@  -L$(ALLOCATORS_ABS_PATH)/hoard -lhoard -lpthread -lm 

$(TEST)-ptmalloc3: $(TEST).c
	$(CC) $(CCFLAGS) $(TEST).c -o $@  -L$(ALLOCATORS_ABS_PATH)/ptmalloc3 -lptmalloc3 -lpthread -lm 


clean:
	rm -f $(TARGET)
