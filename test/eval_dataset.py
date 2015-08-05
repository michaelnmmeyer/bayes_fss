#!/usr/bin/env python3

import sys, math, json
from collections import defaultdict, OrderedDict

SMOOTH = 0.5
NUM_SPLITS = 5

assert len(sys.argv) == 2

positive_label = sys.argv[1]

features = sys.stdin.readline().rstrip("\n")
features = list(filter(None, features.split("\t")))

labels = []
dataset = []

for sample in sys.stdin:
   sample = sample.rstrip("\n")
   fields = list(filter(None, sample.split("\t")))
   assert len(fields) >= 1  # Can have 0 features.
   label, fields = fields[0], fields[1:]
   if not label in labels: # Preserve order to compare with the C version.
      labels.append(label)
   dataset.append((label, fields))

assert len(labels) == 2, "%r" % labels # Only support binary classification for now.
assert positive_label in labels
if positive_label == labels[0]:
   labels.reverse()

def classify(sample, columns, labels_freqs, types):
   probs = {}
   for label, label_freq in labels_freqs.items():
      prob = (label_freq + SMOOTH) / (sum(labels_freqs.values()) + SMOOTH * len(labels))
      probs[label] = math.log2(prob)
   for feat_no, value in enumerate(sample):
      feat_freqs = columns[feat_no][1][value]
      for label, label_freq in labels_freqs.items():
         prob = (feat_freqs[label] + SMOOTH) / (label_freq + SMOOTH * len(types[features[feat_no]]))
         probs[label] += math.log2(prob)
   return max(probs, key=probs.get)

conf_mat = {"tp": 0, "tn": 0, "fp": 0, "fn": 0}

split_size = len(dataset) // NUM_SPLITS

for fold_no in range(NUM_SPLITS):
   test = dataset[fold_no * split_size:(fold_no + 1) * split_size]
   train = dataset[:fold_no * split_size] + dataset[(fold_no + 1) * split_size:]
   columns = [(feature, defaultdict(lambda: defaultdict(int))) for feature in features]
   labels_freqs = defaultdict(int)
   types = defaultdict(set)

   for label, sample in test:
      for feat_no, (_, column) in enumerate(columns):
         column[sample[feat_no]][label] = 0
   for label, sample in train:
      labels_freqs[label] += 1
      for feat_no, (_, column) in enumerate(columns):
         column[sample[feat_no]][label] += 1
         types[features[feat_no]].add(sample[feat_no])

   def print_freqs():
      for feature, column in columns:
         for value, value_freqs in sorted(column.items()):
            value_freqs = "\t".join("%s:%d" % (label, value_freqs[label]) for label in labels)
            print("FOLD:%d\tFEAT:%s\tVAL:%s\t%s" % (fold_no, feature, value, value_freqs))
      for feature in types:
         print("FOLD:%s\tFEAT:%s\tTYP:%d" % (fold_no, feature, len(types[feature])))
      for label, freq in labels_freqs.items():
         print("FOLD:%s\tLABL:%s\tFRQ:%d" % (fold_no, label, freq))

   for real_label, sample in test:
      label = classify(sample, columns, labels_freqs, types)
      if label == positive_label and real_label == positive_label:
         conf_mat["tp"] += 1
      if label == positive_label and real_label != positive_label:
         conf_mat["fp"] += 1
      if label != positive_label and real_label != positive_label:
         conf_mat["tn"] += 1
      if label != positive_label and real_label == positive_label:
         conf_mat["fn"] += 1

precision = conf_mat["tp"] / (conf_mat["tp"] + conf_mat["fp"]) if conf_mat["tp"] + conf_mat["fp"] else 0.
recall = conf_mat["tp"] / (conf_mat["tp"] + conf_mat["fn"]) if conf_mat["tp"] + conf_mat["fn"] else 0.
F1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.
specificity = conf_mat["tn"] / (conf_mat["fp"] + conf_mat["tn"]) if conf_mat["fp"] + conf_mat["tn"] else 0.
accuracy = (conf_mat["tp"] + conf_mat["tn"]) / (conf_mat["tp"] + conf_mat["fp"] + conf_mat["tn"] + conf_mat["fn"]) \
   if conf_mat["tp"] + conf_mat["fp"] + conf_mat["tn"] + conf_mat["fn"] else 0.
AUC = 0.5 * (recall + specificity)

stats = OrderedDict()
stats["accuracy"] = accuracy * 100.
stats["precision"] = precision * 100.
stats["recall"] = recall * 100.
stats["specificity"] = specificity * 100.
stats["F1"] = F1 * 100.
stats["AUC"] = AUC * 100.

print(json.dumps(stats, ensure_ascii=False))

