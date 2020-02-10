#!/bin/bash

source config_ciro.sh

mkdir -p dat
mkdir -p dat/TBLS
mkdir -p dat/TBTT
mkdir -p dat/TBFS
mkdir -p dat/TBCA
mkdir -p dat/LRSN

#crea la prima riga dei file
headrow=alloc
for t in $ALLOC_list; do
		headrow=`echo $headrow $t`
done
echo $headrow


for size in $SIZE_list
do

	DAT1="dat/TBLS/TBLS-$size"; touch $DAT1.dat
	echo $headrow > $DAT1.dat
		for threads in $THREAD_list
		do
		row=$threads
			
			for alloc in $ALLOC_list 
			do
				EX1="./benchmarks/TB_linux-scalability/TB_linux-scalability-$alloc $threads $size"
				#OUT1="${FOLDER}/TBLS-$alloc-sz$size-TH$threads-R$run"; touch $OUT1
				str="b"

				count=0
				TS=0.0
				
				for i in ${FOLDER}/TBLS-$alloc-sz$size-TH$threads-R*; do
					count=$((count+1))
					Tim=`grep "Timer"  $i | head -n1 | cut -d' ' -f4`
					TS=`python -c "print($Tim+$TS)"`
				done
						TS=`python -c "print($TS/$count)"`
						row=`echo $row $TS`
			
			done
			echo $row 	  >> $DAT1.dat
		done
	#done
done
#############################################################################################################
for size in $SIZE_list
do
DAT2="dat/TBTT/TBTT-$size"; touch $DAT2.dat
echo $headrow > $DAT2.dat
	for threads in $THREAD_list
	do
	row=$threads
		
		for alloc in $ALLOC_list 
		do
			EX2="./benchmarks/TB_threadtest/TB_threadtest-$alloc $threads $size"
			#OUT2="${FOLDER}/TBTT-$alloc-sz$size-TH$threads-R$run"; touch $OUT2
			str="b"

			count=0
			TS=0.0
			
			for i in ${FOLDER}/TBTT-$alloc-sz$size-TH$threads-R*; do
				count=$((count+1))
				Tim=`grep "Timer"  $i | head -n1 | cut -d' ' -f4`
				TS=`python -c "print($Tim+$TS)"`
			done
					TS=`python -c "print($TS/$count)"`
					row=`echo $row $TS`
		
		done
		echo $row 	  >> $DAT2.dat
	done
	
done
############################################################################################################################
for size in $SIZE_list
do
	DAT3="dat/LRSN/LRSN-$size"; touch $DAT3.dat
	echo $headrow > $DAT3.dat
	
	#for run in $RUN_list
	#do
		for threads in $THREAD_list
		do
		row=$threads
			
			for alloc in $ALLOC_list 
			do
				EX3="./benchmarks/larson/larson-$alloc 10 `echo $size $((size*16))` 1000 100 1 $threads"
				#OUT3="${FOLDER}/LRSN-$alloc-sz$size-TH$threads-R$run"; touch $OUT3
				str="b"

				count=0
				TS=0.0
				
				for i in ${FOLDER}/LRSN-$alloc-sz$size-TH$threads-R*; do
					count=$((count+1))
					Tim=`grep "Throughput"  $i | tr -s " " | head -n1 | cut -d' ' -f3`
					TS=`python -c "print($Tim+$TS)"`
				done
						TS=`python -c "print($TS/$count)"`
						row=`echo $row $TS`
			
			done
			echo $row 	  >> $DAT3.dat
		done
	#done
done
#######################################################################################################################################
for size in $SIZE_list
do
	DAT4="dat/TBFS/TBFS-$size"; touch $DAT4.dat
	echo $headrow > $DAT4.dat
	
	#for run in $RUN_list
	#do
		for threads in $THREAD_list
		do
		row=$threads
			
			for alloc in $ALLOC_list 
			do
				#EX2="./benchmarks/TB_threadtest/TB_threadtest-$alloc $threads $size"
				#OUT4="${FOLDER}/TBFS-$alloc-sz$size-TH$threads-R$run"; touch $OUT4
				str="b"

				count=0
				TS=0.0
				
				for i in ${FOLDER}/TBFS-$alloc-sz$size-TH$threads-R*; do
					count=$((count+1))
					Tim=`grep "Timer"  $i | head -n1 | cut -d' ' -f4`
					TS=`python -c "print($Tim+$TS)"`
				done
						TS=`python -c "print($TS/$count)"`
						row=`echo $row $TS`
			
			done
			echo $row 	  >> $DAT4.dat
		done
	#done
done
