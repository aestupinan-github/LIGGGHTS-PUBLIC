#!/bin/bash
# Performance of the 125-cube multi-particle case (report section 3.8), surrogate vs superquadric.
# Runs without dumps, 1 thread each, sequential, serial runs pinned to core 2. Results: one line
# per run, columns: variant np rep loop_s pair_s neigh_s comm_s wall_s integrate_s
# Variants: surrogate        = in.pour_125_cubes (batched model, sphere placeholder pair)
#           sq_n1000         = sq_pour_125_cubes.in (real superquadric cube-cube contact)
#           sq_n1000_sphpair = same, pair 'surface default' (same placeholder as the surrogate)
R=/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC
SUR=$R/build/liggghts; SQ=/tmp/opencode/sq_src/build/liggghts
W=/tmp/opencode/perf125; mkdir -p $W; cd $W
export OMP_NUM_THREADS=1 SS_MODELS_DIR=$R/SS_models
nodump() { sed 's/^dump .*//; s/^shell .*//' "$1"; }
nodump $R/test_cases/surrogate_multi/in.pour_125_cubes > surrogate.in
nodump $R/test_cases/superquadric_reference/sq_pour_125_cubes.in > sq_n1000.in
sed 's/surface superquadric$/surface default/' sq_n1000.in > sq_n1000_sphpair.in
row() { # variant np rep log
  L=$4
  loop=$(grep "^Loop time" $L | awk '{print $4}')
  pair=$(grep "^Pair  time" $L | awk '{print $5}')
  neigh=$(grep "^Neigh time" $L | awk '{print $5}')
  comm=$(grep "^Comm  time" $L | awk '{print $5}')
  wall=$(grep -E "^Fix w.. wall/gran time" $L | awk '{s+=$7} END{print s}')
  integ=$(grep "^Fix integrate nve/" $L | awk '{print $7}')
  echo "$1 $2 $3 $loop $pair $neigh $comm $wall $integ"
}
echo "# variant np rep loop_s pair_s neigh_s comm_s wall_s integrate_s"
for rep in 1 2 3; do
  for v in surrogate sq_n1000 sq_n1000_sphpair; do
    B=$SUR; [ $v != surrogate ] && B=$SQ
    taskset -c 2 $B -in $v.in > ${v}_np1_$rep.log 2>&1; row $v 1 $rep ${v}_np1_$rep.log
  done
done
for np in 2 4; do
  for v in surrogate sq_n1000; do
    B=$SUR; [ $v != surrogate ] && B=$SQ
    mpirun --oversubscribe -np $np $B -in $v.in > ${v}_np${np}_1.log 2>&1; row $v $np 1 ${v}_np${np}_1.log
  done
done
