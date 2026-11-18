# Figure 7: execution time of the 125-cube case (serial), stacked by component.
load 'common.gp'
set terminal pngcairo size 1000,600 enhanced font "Sans,11"
set output 'fig07_execution_time_125cubes.png'
set title "125 cubes, 6 walls, 30 000 steps, 1 core: loop time by component"
set ylabel "time per run [s]"
set style data histograms
set style histogram rowstacked
set style fill solid 0.85 border -1
set boxwidth 0.6
set key top right
set yrange [0:*]
set grid ytics
set xtics noenhanced
plot '../data/timing_125cubes.dat' using 4:xtic(2) title "wall contact (fix wall/gran x6)" lc rgb "#1f4e79", \
     '' using 3 title "particle-particle contact (pair)" lc rgb "#d95f02", \
     '' using 5 title "integrator" lc rgb "#7f7f7f", \
     '' using 6 title "rest (neighbour, comm, output, ...)" lc rgb "#c7c7c7"
