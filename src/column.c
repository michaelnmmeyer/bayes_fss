#include <string.h>
#include "column.h"
#include "common.h"
#include "dataset.h"
#include "eval.h"

#define TABLE_INIT_SIZE 4        // We can very well have few attributes, so
                                 // don't make tables too large per default.
#define TABLE_GROWTH_FACTOR 2    // Must produce powers of two.

static struct feature *g_free_list;

static void feature_free(struct feature *feat)
{
   feat->next = g_free_list;
   g_free_list = feat;
}

static uint32_t feature_hash(const void *key)
{
   uint32_t hash = 1315423911U;
   
   for (size_t i = 0; ((const unsigned char *)key)[i]; i++)
      hash ^= (hash << 5) + ((const unsigned char *)key)[i] + (hash >> 2);
   return hash;
}

static bool feature_equal(const void *restrict x, const void *restrict y)
{
   return !strcmp(x, y);
}

// Maybe allocate from a pool?
static struct feature *feature_alloc(const void *key)
{
   size_t key_size = strlen(key) + 1;
   size_t total = offsetof(struct feature, value) + key_size;
   if (total < g_data.feature_size)
      total = g_data.feature_size;
   
   struct feature *feat = xmalloc(total);
   feat->next = NULL;
   memcpy(feat->value, key, key_size);
   
   return feat;
}

static uint32_t linked_feature_hash(const void *key_)
{
   const struct feature *const *key = key_;
   assert(key[0] != key[1]);
   
   uint32_t hash = (uint32_t)(uintptr_t)key[0] + (uint32_t)(uintptr_t)key[1];
   return (hash * 324027) >> 13;
}

static bool linked_feature_equal(const void *restrict x, const void *restrict y)
{
   return !memcmp(x, y, sizeof(struct feature *[2]));
}

static struct feature *linked_feature_alloc(const void *key)
{
   struct feature *feat = g_free_list;
   if (feat)
      g_free_list = feat->next;
   else
      feat = xmalloc(g_data.feature_size);

   feat->next = NULL;
   memcpy(feat->sub_features, key, sizeof(struct feature *[2]));

   return feat;
}

struct table_vtab {
   uint32_t (*hash)(const void *);
   bool (*equal)(const void *restrict, const void *restrict);
   struct feature *(*alloc)(const void *);
};

static const struct table_vtab feature_vtab = {
   .hash = feature_hash,
   .equal = feature_equal,
   .alloc = feature_alloc,
};

static const struct table_vtab linked_feature_vtab = {
   .hash = linked_feature_hash,
   .equal = linked_feature_equal,
   .alloc = linked_feature_alloc,
};

static void table_init(struct table *table, const struct table_vtab *vtab)
{
   *table = (struct table){
      .samples = xmalloc(g_data.num_samples * sizeof *table->samples),
      .table = xcalloc(TABLE_INIT_SIZE, sizeof *table->table),
      .size = TABLE_INIT_SIZE,
      .mask = TABLE_INIT_SIZE - 1,
      .resize_threshold = TABLE_INIT_SIZE * TABLE_GROWTH_FACTOR,
      .vtab = vtab,
   };
}

static void table_resize(struct table *table)
{
   const size_t old_size = table->size;
   struct feature **restrict old_table = table->table;
   
   assert(table->num_types && table->num_types % 2 == 0);
   const size_t new_size = table->num_types;
   const size_t new_mask = new_size - 1;
   struct feature **restrict new_table = xcalloc(new_size, sizeof *new_table);
   
   uint32_t (*hash)(const void *) = table->vtab->hash;
   for (size_t i = 0; i < old_size; i++) {
      struct feature *feat = old_table[i];
      while (feat) {
         struct feature *next = feat->next;
         uint32_t pos = hash(feat->value) & new_mask;
         feat->next = new_table[pos];
         new_table[pos] = feat;
         feat = next;
      }
   }
   
   free(old_table);
   
   table->size = new_size;
   table->mask = new_mask;
   table->table = new_table;
   table->resize_threshold = new_size * TABLE_GROWTH_FACTOR;
}

static struct feature **table_chain(struct table *table, const void *key)
{
   const struct table_vtab *vtab = table->vtab;
   
   uint32_t pos = vtab->hash(key) & table->mask;
   struct feature **feat = &table->table[pos];
   while (*feat && !vtab->equal((*feat)->value, key))
      feat = &(*feat)->next;
   return feat;
}

static void table_add(struct table *table, uint32_t sample_no, const void *key)
{
   struct feature **featp = table_chain(table, key);
   struct feature *feat = *featp;
   
   if (!feat) {
      feat = *featp = table->vtab->alloc(key);
      if (++table->num_types == table->resize_threshold)
         table_resize(table);
   }
   
   table->samples[sample_no] = feat;
}

void column_init(struct column *column, const char *name, uint32_t *links)
{
   column->name = (struct buffer)BUFFER_INIT;
   if (name) {
      buffer_set_json(&column->name, name);
      table_init(&column->table, &feature_vtab);
   } else {
      assert(column == &g_data.columns[g_data.num_features]);
      table_init(&column->table, &linked_feature_vtab);
   }
   column->links = links;
   column->num_links = 0;
   column->state = COL_INACTIVE;
}

void column_add(struct column *column, uint32_t sample_no, const void *key)
{
   table_add(&column->table, sample_no, key);
}

static void table_zero(struct table *table)
{
   size_t size = table->size;
   struct feature **buckets = table->table;
   size_t freqs_size = sizeof(uint32_t[g_data.num_labels]);
   
   for (size_t i = 0; i < size; i++) {
      for (struct feature *feat = buckets[i]; feat; feat = feat->next) {
         feat->seen = false;
         memset(feat->freqs, 0, freqs_size);
      }
   }
}

static uint32_t count_samples(struct feature **samples, size_t start, size_t end)
{
   uint32_t num_types = 0;
   const uint32_t *labels = g_data.samples_labels;
   
   while (start < end) {
      struct feature *sample = samples[start];
      if (!sample->seen) {
         sample->seen = true;
         num_types++;
      }
      sample->freqs[labels[start]]++;
      start++;
   }
   return num_types;
}

uint32_t column_count(struct column *column, size_t test_start, size_t test_end)
{
   assert(column->state == COL_ACTIVE);
   
   table_zero(&column->table);
   
   struct feature **samples = column->table.samples;
   return count_samples(samples, 0, test_start)
        + count_samples(samples, test_end, g_data.num_samples);
}

static void table_clear(struct table *table)
{
   const size_t size = table->size;
   struct feature **buckets = table->table;
   
   for (size_t i = 0; i < size; i++) {
      struct feature *feat = buckets[i];
      while (feat) {
         struct feature *next = feat->next;
         feature_free(feat);
         feat = next;
      }
   }
   
   memset(buckets, 0, size * sizeof *buckets);
   table->num_types = 0;
}

static void table_mutate(struct table *table)
{
   table->vtab = &linked_feature_vtab;
}

static void add_link(uint32_t *links, uint32_t feat_no)
{
   assert(feat_no < g_data.num_features);
   links[feat_no >> 5] |= 1 << (feat_no & 31);
}

static void join_links(struct column *restrict x,
                       const struct column *restrict y,
                       const struct column *restrict z)
{
   for (size_t i = 0; i < g_data.links_size; i++)
      x->links[i] = y->links[i] | z->links[i];

   x->name = y->name;
   add_link(x->links, z - g_data.columns);
   
   x->num_links = y->num_links + z->num_links + 1;
}

void column_join(struct column *restrict x, const struct column *restrict y,
                 const struct column *restrict z)
{
   assert(x != y && y != z && x != z);
   assert(x == &g_data.columns[g_data.num_features]);
   assert(x->vtab == &joined_vtab);
   assert(x->state == COL_ACTIVE);
   assert(y->state == COL_INACTIVE && z->state == COL_INACTIVE);

   join_links(x, y, z);
   
   table_clear(&x->table);

   struct feature **y_samples = y->table.samples;
   struct feature **z_samples = z->table.samples;
   size_t nr = g_data.num_samples;
   
   for (size_t i = 0; i < nr; i++) {
      column_add(x, i, (const struct feature *[]){
         y_samples[i],
         z_samples[i],
      });
   }
}

static void table_fini(struct table *table)
{
   table_clear(table);
   free(table->table);
   free(table->samples);
}

static void merge_links(struct column *restrict x, const struct column *restrict y)
{
   for (size_t i = 0; i < g_data.links_size; i++)
      x->links[i] |= y->links[i];
   
   add_link(x->links, y - g_data.columns);
   
   x->num_links += y->num_links + 1;
}

void column_merge(struct column *restrict x, struct column *restrict y)
{
   assert(x != y);
   assert(x->state == COL_ACTIVE && y->state == COL_INACTIVE);

   merge_links(x, y);
   
   table_clear(&x->table);
   table_clear(&y->table);
   
   table_mutate(&x->table);

   struct feature **x_samples = x->table.samples;
   struct feature **y_samples = y->table.samples;
   size_t nr = g_data.num_samples;
   
   for (size_t i = 0; i < nr; i++) {
      column_add(x, i, (const struct feature *[]){
         x_samples[i],
         y_samples[i],
      });
   }

   table_fini(&y->table);
}
