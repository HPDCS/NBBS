BASE_ALLOCATORS = $(abspath ../../allocators)

PATH_ALLOCATORS = $(subst $(BASE_ALLOCATORS)/Makefile,  , $(wildcard $(BASE_ALLOCATORS)/*))
ALLOCATORS = $(filter-out Makefile nballoc.mk hoard 1lvl-ll, $(subst $(BASE_ALLOCATORS)/,  ,  $(PATH_ALLOCATORS))) kernel-sl
INTERMEDIATE_OBJS_PATH = bin
MY_ALLOCATORS = 1lvl-nb 1lvl-sl 4lvl-nb 4lvl-sl buddy-sl

CC=gcc
CFLAGS= -I$(BASE_ALLOCATORS)/$* -I../../utils 

LIBRARY = -lpthread

SRCS = main.c main.h parameters.h ../../kernel-bd-api/syscall_numbers.h

.SECONDARY:

all:  $(INTERMEDIATE_OBJS_PATH) $(addprefix $(TARGET)-, $(ALLOCATORS))

$(INTERMEDIATE_OBJS_PATH):
	mkdir $(INTERMEDIATE_OBJS_PATH)

$(TARGET)-%-nb: $(SRCS) #$(BASE_ALLOCATORS)/$(TARGET)-%-nb/nballoc.o
	@echo compiling for $@
	$(CC) $(FLAGS) main.c  -I../../utils  -I$(abspath ../../allocators/$*-nb) -L$(abspath ../../allocators/$*-nb) -l$*-nb  -o $(TARGET)-$*-nb -DALLOCATOR=$*-nb -D'TO_BE_REPLACED_MALLOC(x)=bd_xx_malloc(x)' -D'TO_BE_REPLACED_FREE(x)=bd_xx_free(x)' -lpthread -D'ALLOCATOR_NAME="$*-nb"'

$(TARGET)-kernel-sl:  $(SRCS)
	@echo compiling for $@
	$(CC) $(FLAGS) main.c  -I../../utils -o $(TARGET)-kernel-sl -DALLOCATOR=kernel-sl  -lnuma -lpthread -D'ALLOCATOR_NAME="kernel-sl"' -D'KERNEL_BD=1'

$(TARGET)-%-sl:  $(SRCS)
	@echo compiling for $@
	$(CC) $(FLAGS) main.c  -I../../utils  -I$(abspath ../../allocators/$*-sl) -L$(abspath ../../allocators/$*-sl) -l$*-sl -o $(TARGET)-$*-sl -DALLOCATOR=$*-sl -D'TO_BE_REPLACED_MALLOC(x)=bd_xx_malloc(x)' -D'TO_BE_REPLACED_FREE(x)=bd_xx_free(x)' -lpthread -D'ALLOCATOR_NAME="$*-sl"'

$(TARGET)-%: $(TARGET)-%.o  #$(BASE_ALLOCATORS)/%/nballoc.o
	@echo linking $* $(TARGET)-$*.o $(BASE_ALLOCATORS)/$*/nballoc.o ../../utils/utils.o
	$(CC) $(INTERMEDIATE_OBJS_PATH)/$(TARGET)-$*.o  -L$(BASE_ALLOCATORS)/$* -l$* -o $(TARGET)-$* $(LIBRARY)

$(TARGET)-%.o: main.c main.h
	@echo compiling for $@
	$(CC) main.c $(CFLAGS) -c -o bin/$(TARGET)-$*.o -DALLOCATOR=$* -D'TO_BE_REPLACED_MALLOC(x)=malloc(x)' -D'TO_BE_REPLACED_FREE(x)=free(x)' -D'ALLOCATOR_NAME="$*"'

clean:
	-rm $(TARGET)-*
	-rm bin -R

.PHONY: clean
