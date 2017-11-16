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