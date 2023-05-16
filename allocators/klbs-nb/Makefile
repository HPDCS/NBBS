NUM_LEVELS=24
DEBUG=1
CFLAGS=-c -g -Wall -I../../utils -MMD -MP -MF $*.d

include ../nballoc.mk

test: test_stack test_list test_alloc
	@$(foreach test, $^,\
		echo "";\
		echo \#\#\#\# $(test) \#\#\#\# ;\
		./$(test);\
		echo "";\
	)

test_%: test_%.c nballoc.c
	@echo compiling for $@
	$(CC) $(FLAGS) $@.c -g -Wall -I../../utils  -I$(abspath ../../allocators/klbs-nb) -o $@ -lpthread
