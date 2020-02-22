set terminal postscript eps size 5,4 enhanced color font 'cmu10, 22'

if ( tst eq "TBTT" ) set ylabel offset 0 "Seconds (s)" 
if ( tst eq "TBLS" ) set ylabel offset 0 "Seconds (s)" 
if ( tst eq "TBFS" ) set ylabel offset 0 "Seconds (s)" 
if ( tst eq "TBCA" ) set ylabel offset 0 "Seconds (s)" 
if ( tst eq "LRSN" ) set ylabel offset 0 "Throughput (MOps/sec)"

if ( sz eq "4096" ) tsz="4KB"
if ( sz eq "65536" ) tsz="64KB"
if ( sz eq "1048576" ) tsz="1MB"
if ( sz eq "32768" ) tsz="32KB"
if ( sz eq "262144") tsz="256KB"


if ( tst eq "TBTT" ) set title        "Thread test - Bytes=".tsz
if ( tst eq "TBLS" ) set title  "Linux Scalability - Bytes=".tsz
if ( tst eq "TBFS" ) set title "Constant Occupancy - Bytes=".tsz 
if ( tst eq "TBCA" ) set title "Cached Allocation - Bytes=".tsz 
if ( tst eq "LRSN" ) set title             "Larson - Bytes=".tsz

if ( tst eq "LRSN" ) set yrange [-2:*]
if ( tst eq "TBFS" ) set yrange [-2:*]
if ( tst eq "TBLS" ) set yrange [-2:*]
if ( tst eq "TBTT" ) set yrange [-2:*]
if ( tst eq "TBCA" ) set yrange [-2:*]

if ( tst eq "TBTT" ) if ( sz eq "1048576" ) set yrange [-2:1000]




d=2000000000
d=1900000000
if ( tst eq "LRSN" ) d=1000000


set xlabel "#Threads\n"

set size 0.9,0.9
set xtic autofreq 6
set xrange [-1:49]
set key reverse bmargin center horizontal Left

set grid x
set grid y


set output './plots/'.tst.'-'.sz.'.eps'

plot for [col=2:4] './dat/'.tst.'/'.tst.'-'.sz.'.dat' u 1:(column(col)/d)  w lp ls col  t columnheader(col)


#set terminal postscript eps enhanced "Helvetica" 22
#unset xtics
#set xtics (1,2,4,8,16,24,32) #rotate by -45 scale 4 
#set logscale y
#set format y "10^{%S}"
#set key noinvert samplen 1 spacing 2 width 2 height 0 
#set key left top 
#set ylabel offset 1 "-".title."-"
#set termoption dash
#set xrange [3:33]
#set yrange [*:*]
#set title  "Lookahead ".perc
#set title  "Average Event Granularity ".time." {/Symbol m}s"
#seq1=system("awk 'FNR == 1 {print $2}'  ./res/replace.dat")
#seq50=system("awk 'FNR == 1 {print $5}' ./res/replace.dat")
#set style line 5 lt 1 lc rgb "violet" lw 2
#set nokey
#set key inside right top vertical Right noreverse enhanced autotitles columnhead nobox

#plot    './dat/'.tst.'/'.tst.'-'.sz.'.dat'  	u 1:($2/d) 	t "4lvl-nb" 		w lp pt 8 ps 2.5 lw 2 lt 1,\
		''										u 1:($3/d) 	t "1lvl-nb" 		w lp pt 6 ps 2.5 lw 2 lt 1,\
		''										u 1:($4/d) 	t "4lvl-sl" 		w lp pt 8 ps 2.5 lw 2 lt 2 ,\
		''										u 1:($5/d) 	t "1lvl-sl" 		w lp pt 6 ps 2.5 lw 2 lt 2,\
		''										u 1:($6/d) 	t "buddy-sl"		w lp pt 3 ps 2.5 lw 2 lt 1;#\
		                                   																  ,\
		''										u 1:($7/d) 	t "libc" 			w lp pt 2 ps 1.5 lw 2 lt 1,\
		''										u 1:($8/d) 	t "hoard" 			w lp pt 4 ps 1.5 lw 2 lt 1,\
		''										u 1:($4/d) 	t "4lvl-sl" 		w lp pt 8 ps 1.5 lw 2 lt 2,\
		''										u 1:($5/d) 	t "1lvl-sl" 		w lp pt 6 ps 1.5 lw 2 lt 2,\
		''										u 1:($7/d) 	t "libc" 			w lp pt 2 ps 1.5 lw 2 lt 1,\
		''										u 1:($6/d) 	t "ptmalloc3"		w lp pt 5 ps 1.5 lw 2 lt 1,\
		
