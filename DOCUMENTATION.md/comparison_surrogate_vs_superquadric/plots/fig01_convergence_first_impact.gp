load "common.gp"
set output "fig01_convergence_first_impact.png"
set multiplot layout 1,2 title "First impact: superquadric (SQ) converges to the surrogate as corners sharpen"
set logscale x
set xlabel "SQ corner rounding / L   (blockiness 10 ... 1000)"
set xrange [0.0007:0.12]
set key bottom left
F = S."deviation_vs_sq.dat"
# index: 0 c03, 1 c03b (n = 1000 only), 2 c04, 3 c05a, 4 c05b, 5 c05c
set ylabel "|v_z(SQ) - v_z(surrogate)| after impact  [m/s]"
set logscale y
plot F index 0 u 3:4 w lp pt 7 t "03 corner", \
     F index 1 u 3:4 w p pt 6 ps 2 lw 2 t "03b corner, tilt 25° (n=1000)", \
     F index 2 u 3:4 w lp pt 5 t "04 edge", \
     F index 3 u 3:4 w lp pt 9 t "05a spin, h=0.02", \
     F index 4 u 3:4 w lp pt 11 t "05b spin, h=0.05", \
     F index 5 u 3:4 w lp pt 13 t "05c spin, h=0.10"
set ylabel "|ω(SQ) - ω(surrogate)| / |ω(surrogate)| after impact"
plot F index 1 u 3:6 w p pt 6 ps 2 lw 2 lc 2 t "03b corner, tilt 25° (n=1000)", \
     F index 3 u 3:6 w lp pt 9 lc 4 t "05a spin, h=0.02", \
     F index 4 u 3:6 w lp pt 11 lc 5 t "05b spin, h=0.05", \
     F index 5 u 3:6 w lp pt 13 lc 6 t "05c spin, h=0.10"
unset multiplot
