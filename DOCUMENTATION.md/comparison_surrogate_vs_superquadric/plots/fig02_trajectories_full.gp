load "common.gp"
set output "fig02_trajectories_full.png"
set multiplot layout 3,2 title "Centre height over 0.2 s: surrogate (black) vs superquadric n = 1000 (dashed)"
set xlabel "time [s]"
set ylabel "z_c [cm]"
set key top right
do for [c in "c03_corner c03b_corner_tilt25 c04_edge c05a_spin_h002 c05b_spin_h005 c05c_spin_h010"] {
  set title c noenhanced
  plot S."ts_".c."_surrogate.dat" u 2:($5*100) w l ls 1 t "surrogate", \
       S."ts_".c."_sq_n1000.dat"  u 2:($5*100) w l ls 8 t "SQ n=1000"
}
unset multiplot
