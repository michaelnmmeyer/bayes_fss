#include <math.h>
#include <string.h>
#include "eval.h"
#include "column.h"
#include "measure.h"
#include "search.h"
#include "common.h"
#include "eval.h"
#include "cmd.h"

extern struct config g_config;

struct eval g_eval;

static size_t classify(const double *probs)
{
   size_t best_label = 0;
   
   for (size_t label = 1; label < g_data.num_labels; label++)
      if (probs[label] > probs[best_label])
         best_label = label;

   return best_label;
}

static void update_mat_binary(void)
{
   double (*probs)[g_data.num_labels] = g_eval.probs;
   uint32_t positive_label = g_config.positive_label;
   struct conf_mat *mat = g_eval.conf_mat;
   
   for (size_t i = g_eval.test_start; i < g_eval.test_end; i++) {
      size_t label = classify(probs[i - g_eval.test_start]);
      size_t real_label = g_data.samples_labels[i];
      
      if (label == positive_label)
         if (real_label == positive_label)
            mat->true_pos++;
         else
            mat->false_pos++;
      else
         if (real_label == positive_label)
            mat->false_neg++;
         else
            mat->true_neg++;
   }
}

static void update_mat_micro(void)
{
   double (*probs)[g_data.num_labels] = g_eval.probs;
   struct conf_mat *mat = g_eval.conf_mat;
   
   for (size_t i = g_eval.test_start; i < g_eval.test_end; i++) {
      size_t label = classify(probs[i - g_eval.test_start]);
      size_t real_label = g_data.samples_labels[i];
      
      if (label == real_label) {
         mat->true_pos++;
         mat->true_neg += g_data.num_labels - 1;
      } else {
         mat->false_pos++;
         mat->false_neg++;
         mat->true_neg += g_data.num_labels - 2;
      }
   }
}

static void update_mat_macro(void)
{
   double (*probs)[g_data.num_labels] = g_eval.probs;
   struct conf_mat *mat = g_eval.conf_mat;
   
   for (size_t i = g_eval.test_start; i < g_eval.test_end; i++) {
      size_t label = classify(probs[i - g_eval.test_start]);
      size_t real_label = g_data.samples_labels[i];
      
      if (label == real_label) {
         mat[label].true_pos++;
      } else {
         mat[label].false_pos++;
         mat[real_label].false_neg++;
      }
      for (size_t label_no = 0; label_no < g_data.num_labels; label_no++)
         if (label_no != label && label_no != real_label)
            mat[label_no].true_neg++;
   }
}

static void (*update_mat)(void);

void eval_init(void)
{
   // We drop some samples if num_samples is not a multiple of num_folds
   // I don't think this matters much.
   g_eval.fold_size = g_data.num_samples / g_config.num_folds;
   if (!g_eval.fold_size)
      die("not enough samples for evaluation (have %zu, can't perform %zu-fold cross-validation)",
          g_data.num_samples, g_config.num_folds);
   
   g_eval.probs = xmalloc(sizeof(double[g_eval.fold_size][g_data.num_labels]));
   
   g_eval.labels_freqs = xmalloc(g_data.num_labels * sizeof *g_eval.labels_freqs);
   g_eval.types_freqs = xmalloc((g_data.num_features + 1) * sizeof *g_eval.types_freqs);

   if (!strcmp(g_config.classification_mode, "binary")) {
      g_eval.conf_mat_size = sizeof *g_eval.conf_mat;
      update_mat = update_mat_binary;
   } else if (!strcmp(g_config.classification_mode, "multiclass")) {
      if (!strcmp(g_config.averaging_mode, "micro")) {
         g_eval.conf_mat_size = sizeof *g_eval.conf_mat;
         update_mat = update_mat_micro;
      } else if (!strcmp(g_config.averaging_mode, "macro")) {
         g_eval.conf_mat_size = g_data.num_labels * sizeof *g_eval.conf_mat;
         update_mat = update_mat_macro;
      } else {
         die("invalid averaging mode: %s", g_config.averaging_mode);
      }
   } else {
      die("invalid classification mode: %s", g_config.classification_mode);
   }
   g_eval.conf_mat = xmalloc(g_eval.conf_mat_size);
}

static void train(void)
{
   g_eval.num_samples = g_data.num_samples - (g_eval.test_end - g_eval.test_start);

   memset(g_eval.labels_freqs, 0, g_data.num_labels * sizeof *g_eval.labels_freqs);
   for (size_t i = 0; i < g_eval.test_start; i++)
      g_eval.labels_freqs[g_data.samples_labels[i]]++;
   for (size_t i = g_eval.test_end; i < g_data.num_samples; i++)
      g_eval.labels_freqs[g_data.samples_labels[i]]++;
   
   for (size_t feat = 0; feat <= g_data.num_features; feat++) {
      struct column *column = &g_data.columns[feat];
      if (column->state == COL_ACTIVE)
         g_eval.types_freqs[feat] = column_count(column, g_eval.test_start,
                                                 g_eval.test_end);
   }
}

static void compute_priors(void)
{
   double (*probs)[g_data.num_labels] = g_eval.probs;
   size_t num_labels = g_data.num_labels;

   double denom = g_eval.num_samples + g_config.smooth * num_labels;
   for (size_t label = 0; label < num_labels; label++)
      probs[0][label] = log2((g_eval.labels_freqs[label] + g_config.smooth) / denom);

   size_t num_samples = g_eval.test_end - g_eval.test_start;
   for (size_t i = 1; i < num_samples; i++)
      for (size_t label = 0; label < num_labels; label++)
         probs[i][label] = probs[0][label];
}

static void compute_feat_probs(const struct column *column, size_t feat_no)
{
   assert(column->state == COL_ACTIVE);
   
   double (*probs)[g_data.num_labels] = g_eval.probs;
   double div_smooth = g_config.smooth * g_eval.types_freqs[feat_no];
   
   struct feature **samples = column->table.samples;
   for (size_t i = g_eval.test_start; i < g_eval.test_end; i++) {
      const struct feature *feat = samples[i];
      for (size_t label = 0; label < g_data.num_labels; label++) {
         double prob = (feat->freqs[label] + g_config.smooth)
                     / (double)(g_eval.labels_freqs[label] + div_smooth);
         probs[i - g_eval.test_start][label] += log2(prob);
      }
   }
}

static void compute_probs(void)
{
   compute_priors();
   for (size_t feat = 0; feat <= g_data.num_features; feat++) {
      const struct column *column = &g_data.columns[feat];
      if (column->state == COL_ACTIVE)
         compute_feat_probs(column, feat);
   }
}

double eval_model(void)
{
   memset(g_eval.conf_mat, 0, g_eval.conf_mat_size);

   g_eval.test_end = 0;

   for (size_t fold = 0; fold < g_config.num_folds; fold++) {
      g_eval.fold_no = fold;
      g_eval.test_start = g_eval.test_end;
      g_eval.test_end += g_eval.fold_size;
      
      train();
      compute_probs();
      update_mat();
   }

   g_eval.num_evals++;
   return measure_func(g_eval.conf_mat);
}
