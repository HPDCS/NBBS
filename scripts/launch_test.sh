#!/bin/bash

source config.sh

make clean
make NUM_LEVELS=${NUM_LEVELS} MAX=${MAX} MIN=${MIN}

mkdir -p ${FOLDER}


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
				EX4="time -f Real:%e,User:%U,Sys:%S,PCPU:%P,PFAULT:%F,MEM:%K ./benchmarks/TB_fixed-size/TB_fixed-size-$alloc $threads $size" # `echo $((size*16))`"
				EX5="time -f Real:%e,User:%U,Sys:%S,PCPU:%P,PFAULT:%F,MEM:%K ./benchmarks/TB_cached_allocation/TB_cached_allocation-$alloc $threads $size"
				OUT1="${FOLDER}/TBLS-$alloc-sz$size-TH$threads-R$run"; touch $OUT1
				OUT2="${FOLDER}/TBTT-$alloc-sz$size-TH$threads-R$run"; touch $OUT2
				OUT4="${FOLDER}/TBFS-$alloc-sz$size-TH$threads-R$run"; touch $OUT4
				OUT5="${FOLDER}/TBCA-$alloc-sz$size-TH$threads-R$run"; touch $OUT5
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
				
				
				echo TBFS-$alloc sz:$size TH:$threads R:$run --- $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)
				
				if [ `ls -l $OUT4 | awk '{print $5}'` -eq 0 ]
				then
					echo $EX4 TO $OUT4
					($EX4) &> $OUT4
					str="a"
				fi
				
				echo TBCA-$alloc sz:$size TH:$threads R:$run --- $(date +%d)/$(date +%m)/$(date +%Y) - $(date +%H):$(date +%M)
				
				if [ `ls -l $OUT5 | awk '{print $5}'` -eq 0 ]
				then
					echo $EX5 TO $OUT5
					($EX5) &> $OUT5
					str="a"
				fi

				if [ ${str} = "a" ]
				then
					echo ""
				fi
			
			done
		done
	done
done
