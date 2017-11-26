include ../Makefile.inc

#TARGET = $(TEST)-ptmalloc3 $(TEST)-hoard $(TEST)-libc $(TEST)-1lvl-nb $(TEST)-1lvl-sl $(TEST)-4lvl-nb $(TEST)-4lvl-sl
TARGET = $(TEST)-ptmalloc3  $(TEST)-libc $(TEST)-1lvl-nb $(TEST)-1lvl-sl $(TEST)-4lvl-nb $(TEST)-4lvl-sl
ALLOCATORS_ABS_PATH = $(abspath ../../allocators)

all: $(TARGET)  

$(TEST)-libc: $(TEST).cpp
	$(CXX) $(CXXFLAGS) $(TEST).cpp -o $@ -lpthread -lm -D'ALLOCATOR_NAME="libc"'

$(TEST)-hoard: $(TEST).cpp
	$(CXX) $(CXXFLAGS) $(TEST).cpp -o $@  -L$(ALLOCATORS_ABS_PATH)/hoard -lhoard -lpthread -lm  -D'ALLOCATOR_NAME="hoard"'

$(TEST)-ptmalloc3: $(TEST).cpp
	$(CXX) $(CXXFLAGS) $(TEST).cpp -o $@  -L$(ALLOCATORS_ABS_PATH)/ptmalloc3 -lptmalloc3 -lpthread -lm -D'ALLOCATOR_NAME="ptmalloc3"'

$(TEST)-%-nb: $(TEST).cpp
	$(CXX) $(CXXFLAGS) $(TEST).cpp -o $@  -L$(ALLOCATORS_ABS_PATH)/$*-nb -l$*-nb -lpthread -lm -D'ALLOCATOR_NAME="$*-nb"' -D'free(x)=bd_xx_free(x)' -D'malloc(x)=bd_xx_malloc(x)'

$(TEST)-%-sl: $(TEST).cpp
	$(CXX) $(CXXFLAGS) $(TEST).cpp -o $@  -L$(ALLOCATORS_ABS_PATH)/$*-sl -l$*-sl -lpthread -lm -D'ALLOCATOR_NAME="$*-sl"' -D'free(x)=bd_xx_free(x)' -D'malloc(x)=bd_xx_malloc(x)'


clean:
	rm -f $(TARGET)
