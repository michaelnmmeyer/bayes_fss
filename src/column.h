#ifndef BFSS_COLUMN_H
#define BFSS_COLUMN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "buffer.h"

/* While creating a hash table, the following union initially stores features
   values. Features values are no longer needed once the table is built, so
   we discard them and stuff the frequency vectors at the same place. This
   reduces memory usage by about 20%. The "next" pointer must still be
   kept out of the union because we need to traverse the table to zero out
   features frequencies.

   When linking features, we store the original addresses of the joined
   features in "sub_features". Allocated features structures that are no
   longer needed are either reused or marked as dirty, so the pointers in
   "sub_features" do not actually point to the correct sub-features, though
   they do point to a valid memory location. This doesn't matter since we don't
   need to access "sub_features" anymore.
 */
struct feature {
   struct feature *next;
   union {
      struct {
         uint32_t seen;       // Necessary for counting types. Better idea?
         uint32_t freqs[];    // Frequency of each label.
      };
      struct feature *sub_features[2];
      char value[1];
   };
};

struct table_vtab;

struct table {
   struct feature **samples;
   struct feature **table;    // Hash table.
   size_t size;               // Number of buckets.
   size_t mask;               // Hash mask.
   size_t num_types;          // Number of features types.
   size_t resize_threshold;   // Enlarge the table when "num_types" reaches this
                              // value.
   const struct table_vtab *vtab;   // Hash, compare, allocate features.
};

struct column {
   struct buffer name;        // Feature name, encoded as a JSON string.
   uint32_t *links;           // Bitset of features joined with this one.
   size_t num_links;          // Cardinality of the "links" bitset.
   // Column state. TODO: use a stack of column pointers in search.c to avoid
   // messing with this.
   enum {
      COL_INACTIVE,           // Not in the current feature set but can be added
                              // to it.
      COL_MERGED,             // Not in the current feature set and must not
                              // be added to it.
      COL_ACTIVE,             // Part of the current feature set.
   } state;
   struct table table;
};

void column_init(struct column *, const char *label, uint32_t *links);

void column_add(struct column *, uint32_t sample_no, const void *key);

uint32_t column_count(struct column *, size_t test_start, size_t test_end);

void column_join(struct column *restrict, const struct column *restrict,
                 const struct column *restrict);

void column_merge(struct column *restrict, struct column *restrict);

#endif
