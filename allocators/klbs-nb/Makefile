#NUM_LEVELS=24
#DEBUG=1
#CFLAGS=-c -g -Wall -I../../utils -MMD -MP -MF $*.d

include ../nballoc.mk

 

coverage: COV_FLAGS=-fprofile-arcs -ftest-coverage
coverage: clean clean_rep test
	gcov test_alloc.c
	lcov -c --directory . --output-file main_coverage.info
	genhtml main_coverage.info --output-directory out

clean_rep:
	-rm -r out
	-rm *.gcda *.gcov
	-rm main_coverage.info

clean_test: TARGET+=test_stack test_list test_alloc
clean_test: clean


test: TARGET+=test_stack test_list test_alloc
test: test_stack test_list test_alloc
	@$(foreach test, $^,\
		echo "";\
		echo \#\#\#\# $(test) \#\#\#\# ;\
		./$(test);\
		echo "";\
	)

test_%: test_%.c nballoc.c
	@echo compiling for $@
	$(CC) $(FLAGS) $@.c -g -Wall -I../../utils $(COV_FLAGS) -I$(abspath ../../allocators/klbs-nb) -o $@ -lpthread
