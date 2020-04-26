GIT_BENCH = common
SKIP = backup/ common/ 
DIRS = $(wildcard */)


all:
	@$(foreach dir, $(filter-out $(SKIP), $(DIRS) ) ,\
	echo;\
	echo \#\#\#\# COMPILING $(dir) \#\#\#\# ;\
	$(MAKE) -C $(dir) || exit 1;\
	echo \#\#\#\# END COMPILING $(dir) \#\#\#\#;\
	)

clean: 
	@$(foreach dir, $(DIRS),\
		echo "";\
		echo \#\#\#\# CLEANING $(dir) \#\#\#\# ;\
		$(MAKE) -C $(dir) clean;\
		echo \#\#\#\# CLEANING $(dir) \#\#\#\#;\
		echo "";\
	)


.PHONY: all clean
