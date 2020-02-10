#!/bin/bash

THREAD_list="4 8 16 24 32"		#numero di thread
ALLOC_list="4lvl-nb 1lvl-nb 4lvl-sl 1lvl-sl buddy-sl" # hoard
#ALLOC_list="4lvl-nb 1lvl-nb 4lvl-sl 1lvl-sl buddy-sl" # hoard
SIZE_list="8 128 1024"
TEST_list="TBTT LRSN TBLS TBFS"

FOLDER="results"

#make clean
#make NUM_LEVELS=20

rm plots -R
mkdir plots
mkdir -p plots/TBLS
mkdir -p plots/TBTT
mkdir -p plots/TBFS
mkdir -p plots/LRSN

for size in $SIZE_list
do
	for test in $TEST_list
	do
		gnuplot -e "sz='$size'; tst='$test'" ./plot.plt
		mv ./plots/$test/$test-$size.eps ./plots/$test/tmp$test-$size.eps    
		epstopdf ./plots/$test/tmp$test-$size.eps
		mv ./plots/$test/tmp$test-$size.eps ./plots/$test/$test-$size.eps    
		cp ./plots/$test/tmp$test-$size.pdf ./plots/$test/$test-$size.pdf  
		rm ./plots/$test/tmp*  
	done	
	
done
