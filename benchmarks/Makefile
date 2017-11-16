GIT_BENCH = cache-scratch  cache-thrash  common  larson  linux-scalability  phong threadtest
DIRS := $(wildcard */)

all:
	@$(foreach dir, $(DIRS),\
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