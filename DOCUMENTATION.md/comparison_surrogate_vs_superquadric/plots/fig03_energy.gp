load "common.gp"
set output "fig03_energy.png"
set multiplot layout 3,2 title "Total energy (KE + E_{rot} + E_{pot}) / E_0: surrogate (black) vs SQ n = 1000 (dashed)"
set xlabel "time [s]"
set ylabel "E_{tot} / E_0"
set yrange [0:1.05]
set key bottom left
do for [c in "c03_corner c03b_corner_tilt25 c04_edge c05a_spin_h002 c05b_spin_h005 c05c_spin_h010"] {
  set title c noenhanced
  s = S."ts_".c."_surrogate.dat"; q = S."ts_".c."_sq_n1000.dat"
  stats s u 15 every ::0::0 nooutput; E0s = STATS_min
  stats q u 15 every ::0::0 nooutput; E0q = STATS_min
  plot s u 2:($15/E0s) w l ls 1 t "surrogate", q u 2:($15/E0q) w l ls 8 t "SQ n=1000"
}
unset multiplot
