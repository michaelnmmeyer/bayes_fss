#!/usr/bin/env python3

import sys, json

FEATURE_JOIN_STRING = "+"
VALUE_JOIN_STRING = "�"

features_to_select = json.loads("".join(sys.argv[1:]))

features = sys.stdin.readline().rstrip("\n")
features = list(filter(None, features.split("\t")))

assert len(features) == len(set(features))

for feature in features_to_select:
   if isinstance(feature, str):
      feature = [feature]
   for sub_feature in feature:
      assert sub_feature in features, "%s not in %s" % (sub_feature, features)

print("\t" + "\t".join(feature if isinstance(feature, str) else FEATURE_JOIN_STRING.join(feature) \
     for feature in features_to_select))

for sample in sys.stdin:
   sample = sample.rstrip("\n")
   fields = list(filter(None, sample.split("\t")))
   label, values = fields[0], fields[1:]
   
   assert len(values) == len(features)
   values = dict(zip(features, values))
   
   sample_features = []
   for feature in features_to_select:
      if isinstance(feature, str):
         feature = [feature]
      sample_features.append(VALUE_JOIN_STRING.join(values[sub_feature] for sub_feature in feature))
   
   print("%s\t%s" % (label, "\t".join(sample_features)))

