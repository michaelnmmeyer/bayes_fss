#!/usr/bin/env bash

VG="valgrind --leak-check=no --error-exitcode=1"

DATASET=$1

POSITIVE_LABEL=EOS
MEASURE=F1
SMOOTH=0.5

C_RESULTS="results_c.txt"
PY_RESULTS="results_python.txt"

set -o errexit
set -o pipefail

for search_mode in forward backward forward-join backward-join; do
   # Evaluation in C.
   $VG ../bayes_fss -v --search=$search_mode --mode=binary --compact \
                    --measure=$MEASURE --truth=$POSITIVE_LABEL --smooth=$SMOOTH \
                    $DATASET > data/$search_mode.eval_c

   # Evaluation in Python.
   ./extract_subsets.py $MEASURE < data/$search_mode.eval_c | \
   while read subset; do
      echo $subset > /dev/stderr
      ../scripts/mksubset.py $subset < $DATASET | ./eval_dataset.py $POSITIVE_LABEL
   done > data/$search_mode.eval_py
   
   # Compare results.
   ./compare_eval_results.py data/$search_mode.eval_c data/$search_mode.eval_py
   
   # Cleanup
   rm data/$search_mode.eval_c data/$search_mode.eval_py

done
