all:
	$(MAKE) -f submake.mk  THR_FLAGS='-DUSE_TSD_DATA_HACK -D_REENTRANT' OPT_FLAGS='$(OPT_FLAGS)' SYS_FLAGS='$(SYS_FLAGS)' CC='$(CC)' INC_FLAGS='-Isysdeps/pthread -Isysdeps/generic -I.' THR_LIBS=-lpthread

clean:
	$(MAKE) -f submake.mk clean
