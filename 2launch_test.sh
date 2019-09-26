#!/bin/bash

THREAD_list="1 2 4 8 16 24 32"		#numero di thread
RUN_list="1 2 3 4"					#lista del numero di run
#ALLOC_list="hoard 4lvl-nb 1lvl-nb 4lvl-sl 1lvl-sl ptmalloc3 libc"
ALLOC_list="4lvl-sl 4lvl-nb 1lvl-sl 1lvl-nb" #"buddy-sl 4lvl-nb 1lvl-nb 4lvl-sl 1lvl-sl ptmalloc3 libc"
SIZE_list="8 128 1024"


make clean
make NUM_LEVELS=20 MAX=8192 MIN=8

NUM_LEVELS=20
MAX=8192
FOLDER="kernel_results_${NUM_LEVELS}_${MAX}"

mkdir ${FOLDER}


for run in $RUN_list
do
	for size in $SIZE_list
	do
		for alloc in $ALLOC_list 
		do
			for threads in $THREAD_list
			do
				EX1="time -f Real:%e,User:%U,Sys:%S,PCPU:%P,PFAULT:%F,MEM:%K ./benchmarks/TB_linux-scalability/TB_linux-scalability-$alloc $threads $size 			"
				EX2="time -f Real:%e,User:%U,Sys:%S,PCPU:%P,PFAULT:%F,MEM:%K ./benchmarks/TB_threadtest/TB_threadtest-$alloc $threads $size 						"
				EX3="time -f Real:%e,User:%U,Sys:%S,PCPU:%P,PFAULT:%F,MEM:%K ./benchmarks/larson/larson-$alloc 10 `echo $((size-1))` $size 1000 10000 1 $threads		"
				EX4="time -f Real:%e,User:%U,Sys:%S,PCPU:%P,PFAULT:%F,MEM:%K ./benchmarks/TB_fixed-size/TB_fixed-size-$alloc $threads $size `echo $((size*16))`"
				EX5="time -f Real:%e,User:%U,Sys:%S,PCPU:%P,PFAULT:%F,MEM:%K ./benchmarks/TB_same-size/TB_same-size-$alloc $threads $size `echo $((size*16))`"
				OUT1="${FOLDER}/TBLS-$alloc-sz$size-TH$threads-R$run"; touch $OUT1
				OUT2="${FOLDER}/TBTT-$alloc-sz$size-TH$threads-R$run"; touch $OUT2
				OUT3="${FOLDER}/LRSN-$alloc-sz$size-TH$threads-R$run"; touch $OUT3
				OUT4="${FOLDER}/TBFS-$alloc-sz$size-TH$threads-R$run"; touch $OUT4
				#OUT5="${FOLDER}/TBSS-$alloc-sz$size-TH$threads-R$run"; touch $OUT5
				str="b"
				
				echo TBLS-$alloc sz:$size TH:$threads R:$run --- $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)

				if [ `ls -l $OUT1 | awk '{print $5}'` -eq 0 ]
				then
					echo $EX1 TO $OUT1 
					($EX1) &> $OUT1
					str="a"
				fi
				
				echo TBTT-$alloc sz:$size TH:$threads R:$run --- $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)
				
				if [ `ls -l $OUT2 | awk '{print $5}'` -eq 0 ]
				then
					echo $EX2 TO $OUT2
					($EX2) &> $OUT2
					str="a"
				fi
				
				echo LRSN-$alloc sz:$size TH:$threads R:$run --- $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)
				
				if [ `ls -l $OUT3 | awk '{print $5}'` -eq 0 ]
				then
					echo $EX3 TO $OUT3
					($EX3) &> $OUT3
					str="a"
				fi
				
				echo TBFS-$alloc sz:$size TH:$threads R:$run --- $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)
				
				if [ `ls -l $OUT4 | awk '{print $5}'` -eq 0 ]
				then
					echo $EX4 TO $OUT4
					($EX4) &> $OUT4
					str="a"
				fi
				
				#echo TBSS-$alloc sz:$size TH:$threads R:$run --- $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)
				
				#if [ `ls -l $OUT5 | awk '{print $5}'` -eq 0 ]
				#then
				#	echo $EX5 TO $OUT5
				#	($EX5) &> $OUT5
				#	str="a"
				#fi

				#if [ ${str} = "a" ]
				#then
				#	echo ""
				#fi
			
			done
		done
	done
done
