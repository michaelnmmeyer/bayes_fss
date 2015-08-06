#ifndef BFSS_EVAL_H
#define BFSS_EVAL_H

#include "dataset.h"
#include "column.h"
#include "measure.h"

struct config {
   const char *dataset_path;
   const char *positive_label_name;
   const char *classification_mode;
   const char *averaging_mode;
   const char *measure_name;
   const char *search_mode;
   size_t max_links;
   size_t max_features;
   size_t num_folds;
   double smooth;
   bool verbose;
   bool compact_json;

   uint32_t positive_label;
};

struct eval {
   size_t fold_size;
   
   size_t fold_no;         // Current fold number (starting at zero).
   uint32_t num_samples;   // Number of samples in the current train set.
   uint32_t *labels_freqs; // Frequency of each label in the current train set.
   uint32_t *types_freqs;  // Number of types in the current train set.
   
   struct conf_mat *conf_mat;    // Confusion matrix.
   size_t conf_mat_size;         // Size in bytes (for zeroing).
   
   size_t test_start;   // Index of the first sample in the test set.
   size_t test_end;     // Index of the sample following the last sample in the test set.
   void *probs;         // Probabilities: double[max_fold_size][num_labels]
   
   size_t num_evals;
};

extern struct eval g_eval;

void eval_init(void);

double eval_model(void);

#endif
