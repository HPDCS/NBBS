#!/bin/bash

THREAD_list="1 6 12 24 36 48"		#numero di thread
RUN_list="1 2 3 4"					#lista del numero di run
#ALLOC_list="hoard 4lvl-nb 1lvl-nb 4lvl-sl 1lvl-sl ptmalloc3 libc"
ALLOC_list="4lvl-sl 4lvl-nb 1lvl-sl 1lvl-nb buddy-sl" #4lvl-nb 1lvl-nb 4lvl-sl 1lvl-sl ptmalloc3 libc"
SIZE_list="4096 65536 1048576"
TEST_list="TBTT LRSN TBLS TBFS TBCA"

NUM_LEVELS=24
MAX=4194304
MIN=2048

FOLDER="results_${NUM_LEVELS}_${MAX}_${MIN}"



