include ../Makefile.inc

TEST = linux-scalability
TARGET = $(TEST)-hoard $(TEST)

all: $(TARGET)  

$(TEST): $(TEST).c
	$(CC) $(CCFLAGS) $(TEST).c -o $(TEST) -lpthread -lm

$(TEST)-hoard: $(TEST).c
	$(CC) $(CCFLAGS) $(TEST).c -o $(TEST)-hoard  -L../../allocators/Hoard -lhoard -lpthread -lm 


clean:
	rm -f $(TARGET)
