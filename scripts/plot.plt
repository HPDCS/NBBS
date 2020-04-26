set terminal postscript eps size 5,4 enhanced color font 'cmu10, 22'

if ( tst eq "TBTT" ) set ylabel offset 0 "Seconds (s)" 
if ( tst eq "TBLS" ) set ylabel offset 0 "Seconds (s)" 
if ( tst eq "TBFS" ) set ylabel offset 0 "Seconds (s)" 
if ( tst eq "TBCA" ) set ylabel offset 0 "Seconds (s)" 

if ( sz eq "4096" ) tsz="4KB"
if ( sz eq "65536" ) tsz="64KB"
if ( sz eq "1048576" ) tsz="1MB"
if ( sz eq "32768" ) tsz="32KB"
if ( sz eq "262144") tsz="256KB"


if ( tst eq "TBTT" ) set title        "Thread test - Bytes=".tsz
if ( tst eq "TBLS" ) set title  "Linux Scalability - Bytes=".tsz
if ( tst eq "TBFS" ) set title "Constant Occupancy - Bytes=".tsz 
if ( tst eq "TBCA" ) set title "Cached Allocation - Bytes=".tsz 

if ( tst eq "TBFS" ) set yrange [-2:*]
if ( tst eq "TBLS" ) set yrange [-2:*]
if ( tst eq "TBTT" ) set yrange [-2:*]
if ( tst eq "TBCA" ) set yrange [-2:*]

if ( tst eq "TBTT" ) if ( sz eq "1048576" ) set yrange [-2:1000]




d=2000000000
d=1900000000


set xlabel "#Threads\n"

set size 0.9,0.9
set xtic autofreq 6
set xrange [-1:49]
set key reverse bmargin center horizontal Left

set grid x
set grid y


set output './plots/'.tst.'-'.sz.'.eps'

plot for [col=2:4] './dat/'.tst.'/'.tst.'-'.sz.'.dat' u 1:(column(col)/d)  w lp ls col  t columnheader(col)
