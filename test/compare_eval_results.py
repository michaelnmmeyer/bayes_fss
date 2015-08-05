#!/usr/bin/env python3

import sys, json

c_results = open(sys.argv[1])
py_results = open(sys.argv[2])

def load(s):
   try:
      return json.loads(s)
   except:
      print("invalid JSON => %s" % s)
      raise

for i, (line1, line2) in enumerate(zip(c_results, py_results)):
   doc1, doc2 = load(line1), load(line2)
   del doc1["subset"]
   for measure, value in doc1.items():
      assert abs(doc2[measure] - value) < 0.0001, "error at %d (%s)" % (i + 1, measure)
         
assert not c_results.readline()
assert not py_results.readline()

