GIT_BENCH = cache-scratch  cache-thrash  common  larson  linux-scalability  phong threadtest
SKIP = numa_base_test/ common/ 
DIRS = $(wildcard */)

#  || exit 1;\

all:
	@$(foreach dir, $(filter-out $(SKIP), $(DIRS) ) ,\
	echo;\
	echo \#\#\#\# COMPILING $(dir) \#\#\#\# ;\
	$(MAKE) -C $(dir);\
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