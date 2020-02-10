#!/bin/bash

THREAD_list="4 8 16 24 32"		#numero di thread
#RUN_list="1"					#lista del numero di run
#ALLOC_list="4lvl-nb 1lvl-nb 4lvl-sl 1lvl-sl ptmalloc3 libc buddy-sl" # hoard"
ALLOC_list="4lvl-nb 1lvl-nb buddy-sl kernel-sl" # hoard"
SIZE_list="8 256"


MAX_RETRY="10"

NUM_LEVELS=20
MAX=8192
FOLDER="kernel_results_${NUM_LEVELS}_${MAX}"

#make clean
#make NUM_LEVELS=20

mkdir -p kdat
mkdir -p kdat/TBLS
mkdir -p kdat/TBTT
mkdir -p kdat/TBFS
mkdir -p kdat/LRSN
#mkdir ${FOLDER}


#crea la prima riga dei file
headrow=alloc
for t in $ALLOC_list; do
		headrow=`echo $headrow $t`
done
echo $headrow


for size in $SIZE_list
do
	DAT1="kdat/TBLS/TBLS-$size"; touch $DAT1.dat
	echo $headrow > $DAT1.dat
		for threads in $THREAD_list
		do
		row=$threads
			
			for alloc in $ALLOC_list 
			do
				if [ $alloc = "kernel-sl" ]
				then	
					if [ $size = "8" ]
					then 
						sizeT="0"
					else
						sizeT="5"
					fi
				else
					sizeT=$size
				fi
				
				count=0
				TS=0.0
				
				for i in ${FOLDER}/TBLS-$alloc-sz$sizeT-TH$threads-R*; do
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
DAT2="kdat/TBTT/TBTT-$size"; touch $DAT2.dat
echo $headrow > $DAT2.dat
	for threads in $THREAD_list
	do
	row=$threads
		
		for alloc in $ALLOC_list 
		do
			if [ $alloc = "kernel-sl" ]
			then	
				if [ $size = "8" ]
				then 
					sizeT="0"
				else
					sizeT="5"
				fi
			else
				sizeT=$size
			fi

			count=0
			TS=0.0
			
			for i in ${FOLDER}/TBTT-$alloc-sz$sizeT-TH$threads-R*; do
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
	DAT4="kdat/TBFS/TBFS-$size"; touch $DAT4.dat
	echo $headrow > $DAT4.dat
	
	#for run in $RUN_list
	#do
		for threads in $THREAD_list
		do
		row=$threads
			
			for alloc in $ALLOC_list 
			do
				if [ $alloc = "kernel-sl" ]
				then	
					if [ $size = "8" ]
					then 
						sizeT="0"
					else
						sizeT="5"
					fi
				else
					sizeT=$size
				fi

				count=0
				TS=0.0
				
				for i in ${FOLDER}/TBFS-$alloc-sz$sizeT-TH$threads-R*; do
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
############################################################################################################################

exit	

for size in $SIZE_list
do
	DAT3="kdat/LRSN/LRSN-$size"; touch $DAT3.dat
	echo $headrow > $DAT3.dat
	
	#for run in $RUN_list
	#do
		for threads in $THREAD_list
		do
		row=$threads
			
			for alloc in $ALLOC_list 
			do
				if [ $alloc = "kernel-sl" ]
				then	
					if [ $size = "8" ]
					then 
						sizeT="0"
					else
						sizeT="5"
					fi
				else
					sizeT=$size
				fi

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
############################################################################################################################
for size in $SIZE_list
do
	DAT4="kdat/TBSS/TBSS-$size"; touch $DAT4.dat
	echo $headrow > $DAT4.dat
	
	#for run in $RUN_list
	#do
		for threads in $THREAD_list
		do
		row=$threads
			
			for alloc in $ALLOC_list 
			do
				if [ $alloc = "kernel-sl" ]
				then	
					if [ $size = "8" ]
					then 
						sizeT="0"
					else
						sizeT="5"
					fi
				else
					sizeT=$size
				fi

				count=0
				TS=0.0
				
				for i in ${FOLDER}/TBSS-$alloc-sz$sizeT-TH$threads-R*; do
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