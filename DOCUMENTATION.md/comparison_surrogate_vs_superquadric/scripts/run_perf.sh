#!/bin/bash
# 4 cases x {surrogate, sq_n1000} x 5 repeats, sequential (no concurrency), 1 thread each
export OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 SS_MODELS_DIR=/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/SS_models
SUR=/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/build/liggghts
SQ=/tmp/opencode/sq_src/build/liggghts
echo "# case model rep loop_s neigh_s wall_s integrate_s pair_s wall_calls"
for rep in 1 2 3 4 5; do
 for c in test_03_cube_1cm_corner test_03b_cube_1cm_corner_tilt25 test_04_cube_1cm_edge test_05c_cube_1cm_spin_h010; do
  for m in surrogate sq_n1000; do
   B=$SUR; [ $m = sq_n1000 ] && B=$SQ
   ( cd $c/$m && taskset -c 2 $B -in in > out_$rep.log 2>&1 )
   L=$c/$m/out_$rep.log
   loop=$(grep "^Loop time" $L | awk '{print $4}')
   neigh=$(grep "^Neigh time" $L | awk '{print $5}')
   pair=$(grep "^Pair  time" $L | awk '{print $5}')
   wall=$(grep "^Fix wall wall/gran time" $L | awk '{print $7}')
   integ=$(grep "^Fix integrate nve/" $L | awk '{print $7}')
   echo "$c $m $rep $loop $neigh $wall $integ $pair"
  done
 done
done
