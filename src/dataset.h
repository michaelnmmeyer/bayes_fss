#ifndef BFSS_DATASET_H
#define BFSS_DATASET_H

#include <stddef.h>
#include <stdint.h>

struct column;

struct dataset {
   size_t num_labels;
   size_t num_features;
   size_t num_samples;
   char **labels;
   struct column *columns;
   uint32_t *samples_labels;
   size_t feature_size;       // Minimum size of a feature structure.
   size_t links_size;
};

extern struct dataset g_data;

void load_dataset(void);

#endif
