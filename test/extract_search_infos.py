#!/usr/bin/env python3

import sys, json

measure = sys.argv[1]

def dumps_sorted(subset):
   new_subset = [f if isinstance(f, str) else "+".join(sorted(f)) for f in subset]
   new_subset.sort()
   return json.dumps(new_subset)

for line in sys.stdin:
   doc = json.loads(line)
   if "interrupted" in line:
      break
   print("%s\t%f" % (dumps_sorted(doc["subset"]), doc[measure]))

