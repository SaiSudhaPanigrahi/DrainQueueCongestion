#! /bin/sh
file1=dqc_1_bw.txt
file2=dqc_2_bw.txt
file3=dqc_3_bw.txt
gnuplot<<!
set xlabel "time/s" 
set ylabel "rate/kbps"
set xrange [0:300]
set yrange [0:3500]
set term "png"
set output "bw.png"
plot "${file1}" u 1:2 title "flow1" with lines lw 2,\
"${file2}" u 1:2 title "flow2" with lines lw 2,\
"${file3}" u 1:2 title "flow3" with lines lw 2
set output
exit
!


