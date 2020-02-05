GIT_BENCH = cache-scratch  cache-thrash  common  larson  linux-scalability  phong threadtest
SKIP = common/ TB_skeleton/ cache-scratch/  cache-thrash/ linux-scalability/  phong/ threadtest/  TB_same-size/
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
