include ../Makefile.inc

TARGET = $(TEST)-ptmalloc3 $(TEST)-hoard $(TEST)-libc
ALLOCATORS_ABS_PATH = $(abspath ../../allocators)

all: $(TARGET)  

$(TEST)-libc: $(TEST).cpp
	$(CXX) $(CXXFLAGS) $(TEST).cpp -o $@ -lpthread -lm

$(TEST)-hoard: $(TEST).cpp
	$(CXX) $(CXXFLAGS) $(TEST).cpp -o $@  -L$(ALLOCATORS_ABS_PATH)/hoard -lhoard -lpthread -lm 

$(TEST)-ptmalloc3: $(TEST).cpp
	$(CXX) $(CXXFLAGS) $(TEST).cpp -o $@  -L$(ALLOCATORS_ABS_PATH)/ptmalloc3 -lptmalloc3 -lpthread -lm 


clean:
	rm -f $(TARGET)
