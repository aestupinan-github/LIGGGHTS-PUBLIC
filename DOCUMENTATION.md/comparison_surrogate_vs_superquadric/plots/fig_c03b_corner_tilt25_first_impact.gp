load "common.gp"
set output "fig_c03b_corner_tilt25_first_impact.png"
set multiplot layout 4,1 title "Case c03b_corner_tilt25: off-centre corner impact without initial spin, surrogate (black) vs SQ n = 1000 (dashed)" noenhanced
set xrange [13600:13850]
set key outside right
set lmargin 12
P(m) = S."ts_c03b_corner_tilt25_".m.".dat"
unset xlabel
set ylabel "z_c / L"
plot P("sq_n1000") u 1:($5/0.01) w l ls 8 t "SQ n=1000", P("surrogate") u 1:($5/0.01) w l ls 1 t "surrogate"
set ylabel "v_z  [m/s]"
plot P("sq_n1000") u 1:8 w l ls 8 notitle, P("surrogate") u 1:8 w l ls 1 notitle
set ylabel "ω_x  [rad/s]"
plot P("sq_n1000") u 1:9 w l ls 8 notitle, P("surrogate") u 1:9 w l ls 1 notitle
set ylabel "E_{rot}  [J]"
set xlabel "time step  (dt = 1e-5 s)"
plot P("sq_n1000") u 1:13 w l ls 8 notitle, P("surrogate") u 1:13 w l ls 1 notitle
unset multiplot
