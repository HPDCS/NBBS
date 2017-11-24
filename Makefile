DIRS := allocators utils benchmarks

all: 
	@$(foreach dir, $(DIRS),\
	echo;\
	echo \#\#\#\# COMPILING $(dir) \#\#\#\# ;\
	$(MAKE) -C $(dir) $(SUBMAKEFLAGS) || exit 1;\
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