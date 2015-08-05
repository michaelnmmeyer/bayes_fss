#!/usr/bin/env bash

VG="valgrind --leak-check=no --error-exitcode=1"

DATASET=data/cars.tsv
MEASURE=F1
SMOOTH=0.5
AVERAGE=macro

set -o errexit
set -o pipefail

for search_mode in forward backward forward-join backward-join; do
   $VG ../bayes_fss -v --measure=$MEASURE --smooth=$SMOOTH --compact \
      --search=$search_mode --average=$AVERAGE $DATASET | \
         ./extract_search_infos.py $MEASURE > data/$search_mode.search
done

# Floats are not stable, hope this still works.
for search_mode in forward backward forward-join backward-join; do
   cmp data/$search_mode.expect data/$search_mode.search
   rm data/$search_mode.search
done
