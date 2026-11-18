load "common.gp"
set output "fig_c03b_corner_tilt25_first_impact.png"
set terminal pngcairo size 1100,1000 enhanced font "Sans,13" linewidth 1.5
set multiplot layout 5,1 title "Case c03b_corner_tilt25: off-centre corner impact without initial spin\nsurrogate (black, solid) vs SQ n = 1000 (orange, dashed, drawn on top)" noenhanced
set xrange [13600:13850]
set key outside right
set lmargin 12
P(m) = S."ts_c03b_corner_tilt25_".m.".dat"
unset xlabel
set ylabel "z_c / L"
plot P("surrogate") u 1:($5/0.01) w l ls 1 t "surrogate", P("sq_n1000") u 1:($5/0.01) w l ls 9 t "SQ n=1000"
set ylabel "v_z  [m/s]"
plot P("surrogate") u 1:8 w l ls 1 notitle, P("sq_n1000") u 1:8 w l ls 9 notitle
set ylabel "ω_x  [rad/s]"
plot P("surrogate") u 1:9 w l ls 1 notitle, P("sq_n1000") u 1:9 w l ls 9 notitle
set ylabel "E_{rot}  [J]"
plot P("surrogate") u 1:13 w l ls 1 notitle, P("sq_n1000") u 1:13 w l ls 9 notitle
# difference panel: both files share the dense steps around the impact -> paste on step
set ylabel "Δ = surr − SQ"
set xlabel "time step  (dt = 1e-5 s)"
set y2tics; set ytics nomirror
set ylabel "Δv_z [m/s]"; set y2label "Δω_x [rad/s]"
plot "< join ".P("surrogate")." ".P("sq_n1000")." | grep -v '^#'" u 1:($8-$23) w l ls 10 t "Δv_z", \
     "" u 1:($9-$24) axes x1y2 w l ls 9 t "Δω_x"
unset multiplot
