#!/usr/bin/env python3

import sys, json

measure = sys.argv[1]

best_score = -3333.
best_subsets = set()

# Feature names are not sorted in the C output.
def dumps_sorted(subset):
   new_subset = [f if isinstance(f, str) else "@@".join(sorted(f)) for f in subset]
   new_subset.sort()
   return json.dumps(new_subset)

for line in sys.stdin:
   # We might screw up JSON encoding.
   try:
      doc = json.loads(line)
   except:
      print(line, file=sys.stderr)
      raise
   # Last line?
   if "interrupted" in doc:
      break
   if doc[measure] > best_score:
      best_subsets.clear()
   if doc[measure] >= best_score:
      best_subsets.add(dumps_sorted(doc["subset"]))
      best_score = doc[measure]
   print(json.dumps(doc["subset"]))

assert not sys.stdin.readline()

# Check that we actually chose one of the best subsets.
assert doc[measure] == best_score, "%s != %s" % (doc[measure], best_score)
best_subset = dumps_sorted(doc["subset"])
assert best_subset in best_subsets, "%s not in %s" % (best_subset, best_subsets)

