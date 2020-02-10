#!/bin/bash

source config_ciro.sh

mkdir -p plots
rm plots -R
mkdir -p plots

for size in $SIZE_list
do
	for test in $TEST_list
	do
		echo gnuplot -e "sz='$size'; tst='$test'" ./plot_ciro.plt
		gnuplot -e "sz='$size'; tst='$test'" ./plot_ciro.plt
		mv ./plots/$test-$size.eps ./plots/tmp$test-$size.eps    
		epstopdf ./plots/tmp$test-$size.eps
		mv ./plots/tmp$test-$size.eps ./plots/$test-$size.eps    
		cp ./plots/tmp$test-$size.pdf ./plots/$test-$size.pdf  
		rm ./plots/tmp*  
		rm ./plots/*.eps
	done	
	
done
