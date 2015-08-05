#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdalign.h>
#include <stdnoreturn.h>

#include "dataset.h"
#include "buffer.h"
#include "eval.h"
#include "common.h"

struct dataset g_data;

extern struct config g_config;

static FILE *g_file;
static struct buffer g_line = BUFFER_INIT;
static size_t g_line_no;

static void seek_start(void)
{
   rewind(g_file);
   g_line_no = 0;
}

static char *get_line(void)
{
   if (feof(g_file))
      return NULL;

   g_line.size = 0;
   
   int c;
   while ((c = fgetc(g_file)) != EOF && c != '\n') {
      if (g_line.size == g_line.alloc)
         buffer_ensure(&g_line, g_line.size + 1024);
      g_line.data[g_line.size++] = c;
   }
   if (g_line.alloc)
      g_line.data[g_line.size] = '\0';
   
   if (ferror(g_file))
      die("IO error while reading %s: %s", g_config.dataset_path, strerror(errno));
   
   // Skip the last line if empty.
   if (!g_line.size) {
      c = fgetc(g_file);
      if (c == EOF)
         return NULL;
      ungetc(c, g_file);
   }

   g_line_no++;
   return g_line.data;
}

noreturn static void die_loc(const char *format, ...)
{
   extern const char *g_progname;
   
   fprintf(stderr, "%s: invalid format at %s:%zu: ", g_progname,
           g_config.dataset_path, g_line_no);

   va_list ap;
   va_start(ap, format);
   vfprintf(stderr, format, ap);
   va_end(ap);
   
   putc('\n', stderr);
   
   exit(EXIT_FAILURE);
}

#define INVALID_LABEL UINT32_MAX

static uint32_t find_label(const char *label)
{
   for (uint32_t i = 0; i < g_data.num_labels; i++)
      if (!strcmp(g_data.labels[i], label))
         return i;
   return INVALID_LABEL;
}

static void intern_label(const char *label)
{
   static size_t labels_alloc;
   
   if (find_label(label) == INVALID_LABEL) {
      ENLARGE(g_data.labels, g_data.num_labels + 1, labels_alloc, 2);
      g_data.labels[g_data.num_labels++] = xstrdup(label);
   }
}

static size_t read_labels(void)
{
   char *line;
   
   assert(g_line_no == 1);
   while ((line = get_line())) {
      const char *label = strtok(line, "\t");
      if (!label)
         die_loc("no label provided");
      intern_label(label);
   }

   size_t num_samples = g_line_no - 1;
   if (!num_samples)
      die_loc("at least one sample is required");
   
   seek_start();   
   return num_samples;
}

static size_t count_features(void)
{
   assert(g_line_no == 0);
   char *line = get_line();
   if (!line)
      die_loc("empty file");
   
   size_t num_features = 0;
   for (line = strtok(line, "\t"); line; line = strtok(NULL, "\t"))
      num_features++;
   
   return num_features;
}

static void add_sample(size_t sample_no, char *sample)
{
   const char *label = strtok(sample, "\t");
   if (!label)
      die("unexpected error");
   
   uint32_t label_no = find_label(label);
   if (label_no == INVALID_LABEL)
      die("unexpected error");
   
   g_data.samples_labels[sample_no] = label_no;
   
   for (size_t feat_no = 0; feat_no < g_data.num_features; feat_no++) {
      const char *feat = strtok(NULL, "\t");
      if (!feat)
         die_loc("not enough features (expected %zu, found only %zu), maybe there is an empty field?",
                 g_data.num_features, feat_no);
      column_add(&g_data.columns[feat_no], sample_no, feat);
   }
   if (strtok(NULL, "\t"))
      die_loc("excess features (expected merely %zu)", g_data.num_features);
}

static size_t feature_size(size_t num_labels)
{
   size_t size = offsetof(struct feature, freqs) + sizeof(uint32_t[num_labels]);
   return size < sizeof(struct feature) ? sizeof(struct feature) : size;
}

static void alloc_columns(void)
{
   g_data.columns = xmalloc((g_data.num_features + 1) * sizeof *g_data.columns);
   
   g_data.links_size = (g_data.num_features + 1) / 32 + 1;
   uint32_t *links = xcalloc(g_data.num_features + 1, sizeof(uint32_t[g_data.links_size]));
   
   assert(g_line_no == 0);
   char *line = get_line();
   if (!line)
      die_loc("empty file");
   for (size_t i = 0; i < g_data.num_features; i++) {
      const char *feat_name = strtok(i ? NULL : line, "\t");
      if (!feat_name)
         die_loc("read error");
      column_init(&g_data.columns[i], feat_name, links);
      links += g_data.links_size;
   }

   // Sanity check.
   for (size_t i = 0; i < g_data.num_features; i++)
      for (size_t j = i + 1; j < g_data.num_features; j++)
         if (!strcmp(g_data.columns[i].name.data, g_data.columns[j].name.data))
            die_loc("duplicate feature: %s", g_data.columns[i].name.data);

   column_init(&g_data.columns[g_data.num_features], NULL, links);
}

static void load_real(void)
{
   g_data.num_features = count_features();
   g_data.num_samples = read_labels();
   g_data.feature_size = feature_size(g_data.num_labels);

   if (!strcmp(g_config.classification_mode, "binary")) {
      assert(g_config.positive_label_name);
      uint32_t label_no = find_label(g_config.positive_label_name);
      if (label_no == INVALID_LABEL)
         die("positive label '%s' not present in dataset", g_config.positive_label_name);
      g_config.positive_label = label_no;
   }
   
   alloc_columns();  
   
   assert(g_line_no == 1);
   g_data.samples_labels = xmalloc(g_data.num_samples * sizeof *g_data.samples_labels);
   for (size_t i = 0; i < g_data.num_samples; i++) {
      char *line = get_line();
      if (!line)
         die("unexpected error");
      add_sample(i, line);
   }
}

void load_dataset(void)
{
   g_file = fopen(g_config.dataset_path, "r");
   if (!g_file)
      die("can't open dataset at %s: %s", g_config.dataset_path, strerror(errno));
   
   load_real();
   
   fclose(g_file);
   buffer_fini(&g_line);
}
