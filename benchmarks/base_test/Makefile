BASE_ALLOCATORS = ../../allocators

PATH_ALLOCATORS = $(subst $(BASE_ALLOCATORS)/Makefile,  , $(wildcard $(BASE_ALLOCATORS)/*))
ALLOCATORS = $(filter-out Hoard , $(subst $(BASE_ALLOCATORS)/,  ,  $(PATH_ALLOCATORS)))
INTERMEDIATE_OBJS_PATH = bin

CC=gcc
CFLAGS= -I$(BASE_ALLOCATORS)/$* -I../../utils


TARGET=base_test

.SECONDARY:

all:  $(INTERMEDIATE_OBJS_PATH) $(addprefix $(TARGET)-, $(ALLOCATORS))

$(INTERMEDIATE_OBJS_PATH):
	mkdir $(INTERMEDIATE_OBJS_PATH)

$(TARGET)-%: $(TARGET)-%.o  $(BASE_ALLOCATORS)/%/nballoc.o
	@echo linking $* $(TARGET)-$*.o $(BASE_ALLOCATORS)/$*/nballoc.o ../../utils/utils.o
	$(CC) $(INTERMEDIATE_OBJS_PATH)/$(TARGET)-$*.o $(BASE_ALLOCATORS)/$*/nballoc.o ../../utils/utils.o -o $(TARGET)-$*

$(TARGET)-%.o: main.c
	@echo compiling for $(TARGET)-$*.o
	$(CC) main.c $(CFLAGS) -c -o bin/$(TARGET)-$*.o


clean:
	rm $(TARGET)-*
	rm bin -R

.PHONY: clean
