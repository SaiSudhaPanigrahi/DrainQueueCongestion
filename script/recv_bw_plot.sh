#! /bin/sh
instance=1
algo=viva
data_dir=${algo}"/"
file1=${data_dir}${instance}_${algo}_1_dur_bw.txt
file2=${data_dir}${instance}_${algo}_2_dur_bw.txt
file3=${data_dir}${instance}_${algo}_3_dur_bw.txt
output=${algo}-${instance}
gnuplot<<!
set xlabel "time/s" 
set ylabel "rate/kbps"
set xrange [0:300]
set yrange [0:5000]
set grid
set term "png"
set output "${output}-recv-bw.png"
plot "${file1}" u 1:2 title "flow1" with lines lw 2 lc 1,\
"${file2}" u 1:2 title "flow2" with lines lw 2 lc 2,\
"${file3}" u 1:2 title "flow3" with lines lw 2 lc 3
set output
exit
!


