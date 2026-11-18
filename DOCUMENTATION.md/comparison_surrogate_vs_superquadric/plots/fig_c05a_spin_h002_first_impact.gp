load "common.gp"
set output "fig_c05a_spin_h002_first_impact.png"
set multiplot layout 3,1 title "Case c05a_spin_h002: first impact, surrogate (black) vs superquadric blockiness 10…1000 (n = 1000 purple dashed, on top)" noenhanced
set xrange [4900:5100]
set key outside right
set lmargin 12
P(m) = S."ts_c05a_spin_h002_".m.".dat"
set ylabel "z_c / L"
unset xlabel
plot P("surrogate") u 1:($5/0.01) w l ls 1 t "surrogate", \
     P("sq_n10")   u 1:($5/0.01) w l ls 2 t "SQ n=10", \
     P("sq_n20")   u 1:($5/0.01) w l ls 3 t "SQ n=20", \
     P("sq_n50")   u 1:($5/0.01) w l ls 4 t "SQ n=50", \
     P("sq_n100")  u 1:($5/0.01) w l ls 5 t "SQ n=100", \
     P("sq_n200")  u 1:($5/0.01) w l ls 6 t "SQ n=200", \
     P("sq_n400")  u 1:($5/0.01) w l ls 7 t "SQ n=400", \
     P("sq_n1000") u 1:($5/0.01) w l ls 8 t "SQ n=1000"
set ylabel "v_z  [m/s]"
plot P("surrogate") u 1:8 w l ls 1 notitle, \
     P("sq_n10")   u 1:8 w l ls 2 notitle, P("sq_n20") u 1:8 w l ls 3 notitle, P("sq_n50") u 1:8 w l ls 4 notitle, \
     P("sq_n100")  u 1:8 w l ls 5 notitle, P("sq_n200") u 1:8 w l ls 6 notitle, P("sq_n400") u 1:8 w l ls 7 notitle, \
     P("sq_n1000") u 1:8 w l ls 8 notitle
set ylabel "|ω|  [rad/s]"
set xlabel "time step  (dt = 1e-5 s)"
W(i,j,k) = sqrt(i*i+j*j+k*k)
plot P("surrogate") u 1:(W($9,$10,$11)) w l ls 1 notitle, \
     P("sq_n10")   u 1:(W($9,$10,$11)) w l ls 2 notitle, P("sq_n20") u 1:(W($9,$10,$11)) w l ls 3 notitle, \
     P("sq_n50")   u 1:(W($9,$10,$11)) w l ls 4 notitle, P("sq_n100") u 1:(W($9,$10,$11)) w l ls 5 notitle, \
     P("sq_n200")  u 1:(W($9,$10,$11)) w l ls 6 notitle, P("sq_n400") u 1:(W($9,$10,$11)) w l ls 7 notitle, \
     P("sq_n1000") u 1:(W($9,$10,$11)) w l ls 8 notitle
unset multiplot
