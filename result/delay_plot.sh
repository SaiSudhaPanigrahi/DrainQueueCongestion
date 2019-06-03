#! /bin/sh
file1=dqc_1_owd.txt
file2=dqc_2_owd.txt
file3=dqc_3_owd.txt
gnuplot<<!
set xlabel "time/s" 
set ylabel "delay/ms"
set xrange [0:300]
set yrange [100:450]
set term "png"
set output "delay.png"
plot "${file1}" u 1:3 title "flow1" with lines lw 2,\
"${file2}" u 1:3 title "flow2" with lines lw 2,\
"${file3}" u 1:3 title "flow3" with lines lw 2
set output
exit
!


