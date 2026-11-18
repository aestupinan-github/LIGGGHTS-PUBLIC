load "common.gp"
set output "fig_c04_edge_first_impact.png"
set multiplot layout 3,1 title "Case c04_edge: first impact, surrogate (black) vs superquadric blockiness 10…1000" noenhanced
set xrange [13680:13880]
set key outside right
set lmargin 12
P(m) = S."ts_c04_edge_".m.".dat"
set ylabel "z_c / L"
unset xlabel
plot P("sq_n10")   u 1:($5/0.01) w l ls 2 t "SQ n=10", \
     P("sq_n20")   u 1:($5/0.01) w l ls 3 t "SQ n=20", \
     P("sq_n50")   u 1:($5/0.01) w l ls 4 t "SQ n=50", \
     P("sq_n100")  u 1:($5/0.01) w l ls 5 t "SQ n=100", \
     P("sq_n200")  u 1:($5/0.01) w l ls 6 t "SQ n=200", \
     P("sq_n400")  u 1:($5/0.01) w l ls 7 t "SQ n=400", \
     P("sq_n1000") u 1:($5/0.01) w l ls 8 t "SQ n=1000", \
     P("surrogate") u 1:($5/0.01) w l ls 1 t "surrogate"
set ylabel "v_z  [m/s]"
plot P("sq_n10")   u 1:8 w l ls 2 notitle, P("sq_n20") u 1:8 w l ls 3 notitle, P("sq_n50") u 1:8 w l ls 4 notitle, \
     P("sq_n100")  u 1:8 w l ls 5 notitle, P("sq_n200") u 1:8 w l ls 6 notitle, P("sq_n400") u 1:8 w l ls 7 notitle, \
     P("sq_n1000") u 1:8 w l ls 8 notitle, P("surrogate") u 1:8 w l ls 1 notitle
set ylabel "|ω|  [rad/s]"
set xlabel "time step  (dt = 1e-5 s)"
W(i,j,k) = sqrt(i*i+j*j+k*k)
plot P("sq_n10")   u 1:(W($9,$10,$11)) w l ls 2 notitle, P("sq_n20") u 1:(W($9,$10,$11)) w l ls 3 notitle, \
     P("sq_n50")   u 1:(W($9,$10,$11)) w l ls 4 notitle, P("sq_n100") u 1:(W($9,$10,$11)) w l ls 5 notitle, \
     P("sq_n200")  u 1:(W($9,$10,$11)) w l ls 6 notitle, P("sq_n400") u 1:(W($9,$10,$11)) w l ls 7 notitle, \
     P("sq_n1000") u 1:(W($9,$10,$11)) w l ls 8 notitle, P("surrogate") u 1:(W($9,$10,$11)) w l ls 1 notitle
unset multiplot
