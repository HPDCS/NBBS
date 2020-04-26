#!/bin/bash

source config.sh

mkdir -p dat
mkdir -p dat/TBLS
mkdir -p dat/TBTT
mkdir -p dat/TBFS
mkdir -p dat/TBCA


default=$((30*1900000000))

#crea la prima riga dei file
headrow=alloc
for t in $ALLOC_list; do
		headrow=`echo $headrow $t`
done
echo $headrow

for test in $TEST_list
do
	for size in $SIZE_list
	do

		DAT1="dat/$test/$test-$size"; touch $DAT1.dat
		echo $headrow > $DAT1.dat
			for threads in $THREAD_list
			do
			row=$threads
				
				for alloc in $ALLOC_list 
				do
					str="b"

					count=1
					TS=$default
					if test -f "${FOLDER}/$test-$alloc-sz$size-TH$threads-R1"; then
						count=0
						TS=0
						for i in ${FOLDER}/$test-$alloc-sz$size-TH$threads-R*; do
							count=$((count+1))
							if [ "$test" == "LRSN" ]; then
								Tim=`grep "Throughput"  $i | tr -s " " | head -n1 | cut -d' ' -f3`
							else
								Tim=`grep "Timer"  $i | head -n1 | cut -d' ' -f4`
							fi
							
							if [ "$alloc" == "kernel-sl" ]; then
								echo $i `grep "total ops done" $i | cut -d' ' -f6-` `grep "Real" $i | cut -d',' -f1` | grep " 0 R"
							fi
							
							TS=`python -c "print($Tim+$TS)"`
						done
					fi
							TS=`python -c "print($TS/$count)"`
							row=`echo $row $TS`
				
				done
				echo $row 	  >> $DAT1.dat
			done
	done
done
