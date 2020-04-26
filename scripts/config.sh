#!/bin/bash

THREAD_list="1 6 12 24 36 48"		# thread list
RUN_list="1 2 3 4"					# run list
ALLOC_list="1lvl-nb 4lvl-nb"        # kernel-sl # list of allocators
SIZE_list="4096 32768 262144"
TEST_list="TBTT TBLS TBFS TBCA"

NUM_LEVELS=24
MAX=4194304
MIN=2048

FOLDER="results_${NUM_LEVELS}_${MAX}_${MIN}"



