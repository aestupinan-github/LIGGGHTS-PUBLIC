#!/bin/bash
d=$1
cd $d
if [[ $d == *surrogate ]]; then BIN=/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/build/liggghts; else BIN=/tmp/opencode/sq_src/build/liggghts; fi
SS_MODELS_DIR=/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/SS_models timeout 3000 $BIN -in in > out.log 2>&1
echo "$d exit=$? err=$(grep -c '^ERROR' out.log)"
