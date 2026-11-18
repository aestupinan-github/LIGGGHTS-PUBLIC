# Common settings for all comparison figures. Run from the plots/ folder:  gnuplot fig_XX.gp
set terminal pngcairo size 1100,750 enhanced font "Sans,13" linewidth 1.5
set datafile commentschars "#"
set grid lc rgb "#dddddd"
set key top right box opaque
S = "../data/"
# line styles: surrogate black, SQ blockiness = colour ramp
set style line 1 lc rgb "black"   lw 3 dt 1
set style line 2 lc rgb "#d62728" lw 1.5
set style line 3 lc rgb "#ff7f0e" lw 1.5
set style line 4 lc rgb "#bcbd22" lw 1.5
set style line 5 lc rgb "#2ca02c" lw 1.5
set style line 6 lc rgb "#17becf" lw 1.5
set style line 7 lc rgb "#1f77b4" lw 1.5
set style line 8 lc rgb "#9467bd" lw 2 dt 2
