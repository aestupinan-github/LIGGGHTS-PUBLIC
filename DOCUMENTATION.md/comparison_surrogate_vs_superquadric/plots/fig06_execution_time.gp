load "common.gp"
set output "fig06_execution_time.png"
set terminal pngcairo size 1300,600 enhanced font "Sans,13" linewidth 1.5
set multiplot layout 1,2 title "Execution time, 20 000 steps, 1 particle, 1 core (mean of 5, equal neighbour bins, warm = steady state)"
F = S."timing_plot.dat"
set style fill solid 0.85 border -1
set boxwidth 0.36
set xtics ("03 corner" 0, "03b tilt 25°" 1, "04 edge" 2, "05c spin" 3)
set xrange [-0.6:3.6]
set yrange [0:125]
set ylabel "loop time per run  [ms]"
set key top left maxrows 3 font ",11"
set title "Loop time: wall-contact fix (dark) + rest (light)"
plot F u ($1-0.19):($6+$5)  w boxes lc rgb "#bbbbbb" t "surrogate, warm: rest", \
     F u ($1-0.19):5        w boxes lc rgb "black"   t "surrogate, warm: wall contact", \
     F u ($1+0.19):($9+$8)  w boxes lc rgb "#f4b183" t "SQ n=1000: rest", \
     F u ($1+0.19):8        w boxes lc rgb "#e6550d" t "SQ n=1000: wall contact", \
     F u ($1-0.19):10       w p pt 1 ps 2.5 lw 2.5 lc rgb "#1f77b4" t "surrogate, cold (1st run): total"
set title "Wall-contact fix time vs number of surrogate model calls"
set xtics auto
set xrange [0:1150]
set yrange [0:40]
set xlabel "surrogate model calls per run"
set ylabel "wall-contact fix time per run  [ms]"
set key top left maxrows 3 font ",11"
fw(x) = aw + bw*x
fc(x) = ac + bc*x
aw = 3; bw = 0.015; ac = 19; bc = 0.015
fit fw(x) F u 3:5 via aw, bw
fit fc(x) F u 3:11 via ac, bc
set label 1 sprintf("warm: %.2f ms + %.1f µs/call", aw, bw*1000) at 420,9  tc rgb "black"
set label 2 sprintf("cold: %.2f ms + %.1f µs/call", ac, bc*1000) at 420,31 tc rgb "#1f77b4"
plot F u 3:11 w p pt 9 ps 2 lc rgb "#1f77b4" t "surrogate, cold", fc(x) w l lc rgb "#1f77b4" dt 2 notitle, \
     F u 3:5  w p pt 7 ps 2 lc rgb "black"   t "surrogate, warm", fw(x) w l lc rgb "black" dt 2 notitle, \
     F u 3:8  w p pt 5 ps 2 lc rgb "#e6550d" t "SQ n=1000 (x = surrogate calls of the same case)"
unset multiplot
