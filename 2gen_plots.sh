#!/bin/bash

THREAD_list="1 2 4 8 16 24 32"		#numero di thread
#RUN_list="1"					#lista del numero di run
#ALLOC_list="4lvl-nb 1lvl-nb 4lvl-sl 1lvl-sl ptmalloc3 libc buddy-sl" # hoard
ALLOC_list="4lvl-nb 1lvl-nb buddy-sl kernel-sl" # hoard
SIZE_list="8 256"
TEST_list="TBTT TBLS TBFS TBSS LRSN"

#make clean
#make NUM_LEVELS=20

rm kplots -R
mkdir kplots
mkdir kplots/TBLS
mkdir kplots/TBTT
mkdir kplots/TBFS
mkdir kplots/LRSN

for size in $SIZE_list
do

	for test in $TEST_list
	do
		gnuplot -e "sz='$size'; tst='$test'" ./plot.plt
		epstopdf ./plots/$test/$test-$size.eps  
	done	
	
done
