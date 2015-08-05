#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdnoreturn.h>

#include "search.h"
#include "dataset.h"
#include "buffer.h"
#include "measure.h"
#include "common.h"
#include "eval.h"

extern struct config g_config;
extern struct eval g_eval;

static double g_stop;            // Termination flag.

#define INVALID_SCORE -333.
static double g_best_score = INVALID_SCORE;

static bool feature_active(const uint32_t *set, uint32_t feat_no)
{
   assert(feat_no < g_data.num_features);
   return set[feat_no >> 5] & (1 << (feat_no & 31));
}

static const char *current_subset(void)
{
   static struct buffer buf = BUFFER_INIT;
   
   struct column *cols = g_data.columns;
   size_t nr = g_data.num_features;
   
   buffer_clear(&buf);   
   buffer_catc(&buf, '[');
   
   for (size_t i = 0; i <= nr; i++) {
      const struct column *col = &cols[i];
      if (col->state != COL_ACTIVE)
         continue;
      if (!col->num_links) {
         buffer_cat(&buf, col->name.data, col->name.size);
      } else {
         buffer_catc(&buf, '[');
         buffer_cat(&buf, col->name.data, col->name.size);
         for (size_t j = 0; j < nr; j++) {
            if (feature_active(col->links, j)) {
               buffer_catc(&buf, ',');
               buffer_cat(&buf, cols[j].name.data, cols[j].name.size);
            }
         }
         buffer_catc(&buf, ']');
      }
      buffer_catc(&buf, ',');
   }
   // Remove the trailing comma.
   if (buf.size > 1)
      buf.size--;
   buffer_catc(&buf, ']');
   
   return buf.data;
}

static char g_step_report[] = 
"{\n"
"   \"subset\": %s,\n"
"   \"accuracy\": %f,\n"
"   \"precision\": %f,\n"
"   \"recall\": %f,\n"
"   \"F1\": %f\n"
"}\n"
;
static double eval_model_verbose(void)
{
   double ret = eval_model();
   
   struct measures stats;
   full_eval(&stats, g_eval.conf_mat);
   
   printf(g_step_report,
      current_subset(),
      stats.accuracy * 100.,
      stats.precision * 100.,
      stats.recall * 100.,
      stats.F1 * 100.
   );
   
   return ret;
}

static char g_full_report[] = {
"{\n"
"   \"subset\": %s,\n"
"   \"accuracy\": %f,\n"
"   \"precision\": %f,\n"
"   \"recall\": %f,\n"
"   \"F1\": %f,\n"
"   \"subsets_evaluated\": %zu,\n"
"   \"interrupted\": %s\n"
"}\n"
};
noreturn static void print_best(void)
{
   if (g_best_score == INVALID_SCORE) {
      // Not enough time to do anything.
      exit(EXIT_SUCCESS);
   }

   eval_model();
   struct measures stats;
   full_eval(&stats, g_eval.conf_mat);

   printf(g_full_report,
      current_subset(),
      stats.accuracy * 100.,
      stats.precision * 100.,
      stats.recall * 100.,
      stats.F1 * 100.,
      g_eval.num_evals,
      g_stop ? "true" : "false");

   exit(EXIT_SUCCESS);
}

static void compress_json(char *json)
{
   assert(*json && json[strlen(json) - 1] == '\n');
   
   size_t i = 0;
   
   for (size_t j = 0; json[j]; j++) {
      unsigned char c = json[j];
      if (!isspace(c))
         json[i++] = c;
   }
   
   json[i++] = '\n';
   json[i] = '\0';
}

static double (*run_eval)(void) = eval_model;

static void activate_features(void)
{
   for (size_t i = 0; i < g_data.num_features; i++)
      g_data.columns[i].state = COL_ACTIVE;
}

static void search_none(void)
{
   activate_features();
   g_best_score = run_eval();
}

static void search_forward(void)
{
   g_best_score = run_eval();

   size_t num_active_features = 0;
   size_t num_features = g_data.num_features;
   struct column *columns = g_data.columns;

   size_t max_features = g_config.max_features;
   while (num_active_features < max_features) {
      num_active_features++;
      double cur_best = 0.;
      struct column *to_add = NULL;
      
      for (size_t i = 0; i < num_features && !g_stop; i++) {
         struct column *col = &columns[i];
         if (col->state == COL_ACTIVE)
            continue;
         col->state = COL_ACTIVE;
         double cur = run_eval();
         col->state = COL_INACTIVE;
         if (cur >= cur_best) {
            cur_best = cur;
            to_add = col;
         }
      }
      
      /* Ideally, if we obtain exactly the same score with the current subset
         and the best seen so far, we should select the simpler one (the one
         from the previous iteration. But this leads to a pathological case on a
         real dataset (cars.tsv): we end up with the empty feature set. The
         clean solution is to store the simpler subset, continue, and restore it
         afterwards if no improvement can be made. For now, we adopt the dirty
         solution, which is to choose a more complex subset.
       */
      if (g_stop || cur_best < g_best_score)
         break;
      to_add->state = COL_ACTIVE;
      g_best_score = cur_best;
   }
}

static void search_backward(void)
{
   activate_features();
   g_best_score = run_eval();
   
   size_t num_features = g_data.num_features;
   size_t num_active_features = num_features;
   struct column *columns = g_data.columns;
   
   while (num_active_features) {
      num_active_features--;
      double cur_best = 0.;
      struct column *to_remove = NULL;
      
      for (size_t i = 0; i < num_features && !g_stop; i++) {
         struct column *col = &columns[i];
         if (col->state == COL_INACTIVE)
            continue;
         col->state = COL_INACTIVE;
         double cur = run_eval();
         col->state = COL_ACTIVE;
         if (cur >= cur_best) {
            cur_best = cur;
            to_remove = col;
         }
      }
      
      if (g_stop || cur_best < g_best_score)
         break;
      // If even, prefer the shorter subset.
      to_remove->state = COL_INACTIVE;
      g_best_score = cur_best;
   }
}

static void search_forward_join(void)
{
   g_best_score = run_eval();

   size_t num_active_features = 0;
   size_t num_features = g_data.num_features;
   struct column *columns = g_data.columns;
   struct column *join_column = &columns[num_features];
   size_t max_links = g_config.max_links;
   
   size_t max_features = g_config.max_features;
   
   while (num_active_features < max_features) {
      num_active_features++;
      double cur_best = 0.;
      struct column *to_add = NULL;
      struct column *to_merge = NULL;
      
      // Addition of a new feature.
      for (size_t i = 0; i < num_features && !g_stop; i++) {
         struct column *col = &columns[i];
         if (col->state != COL_INACTIVE)
            continue;
         col->state = COL_ACTIVE;
         double cur = run_eval();
         col->state = COL_INACTIVE;
         if (cur >= cur_best) {
            cur_best = cur;
            to_add = col;
         }
      }
      
      // Merging of an inactive feature with an active one.
      join_column->state = COL_ACTIVE;
      for (size_t i = 0; i < num_features && !g_stop; i++) {
         struct column *col1 = &columns[i];
         if (col1->state != COL_ACTIVE || col1->num_links >= max_links)
            continue;
         col1->state = COL_INACTIVE;
         for (size_t j = 0; j < num_features; j++) {
            struct column *col2 = &columns[j];
            if (col2 == col1 || col2->state != COL_INACTIVE)
               continue;
            column_join(join_column, col1, col2);
            double cur = run_eval();
            if (cur >= cur_best) {
                cur_best = cur;
                to_add = col1;
                to_merge = col2;
            }
         }
         col1->state = COL_ACTIVE;
      }
      join_column->state = COL_INACTIVE;
      
      if (g_stop || cur_best < g_best_score)
         break;
         
      if (!to_merge) {
         to_add->state = COL_ACTIVE;
      } else {
         column_merge(to_add, to_merge);
         to_merge->state = COL_MERGED;
      }
      g_best_score = cur_best;
   }
}

/* Pazzani 1996, BSEJ algorithm.
 * As concerns removing attributes, there are two possibilities:
 *    1. Remove a joined attribute in one step: A BC -> (A, BC)
 *    2. Try to decompose a joined attribute:   A BC -> (AB, BC, AC)
 * I think that the method the author had in mind is the first one, since the
 * other one introduces potential cycles, and, in some way, amounts to taking
 * a step back in the search process. The second method could also be useful,
 * but requires a) to check for cycles b) to preserve the original columns (and
 * maybe also the most recently joined column, for increased efficiency).
 */
static void search_backward_join(void)
{
   activate_features();
   g_best_score = run_eval();
   
   struct column *columns = g_data.columns;
   size_t num_features = g_data.num_features;
   size_t num_active_features = num_features;
   struct column *join_column = &columns[num_features];
   size_t max_links = g_config.max_links;;
   
   while (num_active_features) {
      num_active_features--;
      double cur_best = 0.;
      struct column *to_remove = NULL;
      struct column *to_merge = NULL;
      
      // Removal of an active feature.
      for (size_t i = 0; i < num_features && !g_stop; i++) {
         struct column *column = &columns[i];
         if (column->state != COL_ACTIVE)
            continue;
         column->state = COL_INACTIVE;
         double cur = run_eval();
         column->state = COL_ACTIVE;
         if (cur >= cur_best) {
            cur_best = cur;
            to_remove = column;
         }
      }
      
      // Merging of two active features.
      join_column->state = COL_ACTIVE;
      for (size_t i = 0; i < num_features && !g_stop; i++) {
         struct column *col1 = &columns[i];
         if (col1->state != COL_ACTIVE || col1->num_links >= max_links)
            continue;
         col1->state = COL_INACTIVE;
         for (size_t j = i + 1; j < num_features; j++) {
            struct column *col2 = &columns[j];
            if (col2->state != COL_ACTIVE || col1->num_links + col2->num_links >= max_links)
               continue;
            col2->state = COL_INACTIVE;
            column_join(join_column, col2, col1);
            double cur = run_eval();
            col2->state = COL_ACTIVE;
            if (cur >= cur_best) {
                cur_best = cur;
                to_merge = col1;
                to_remove = col2;
            }
         }
         col1->state = COL_ACTIVE;
      }
      join_column->state = COL_INACTIVE;

      if (g_stop || cur_best < g_best_score)
         break;

      to_remove->state = COL_INACTIVE;
      if (to_merge)
         column_merge(to_merge, to_remove);
      g_best_score = cur_best;
   }
}

static void (*search_method(void))(void)
{
   const struct {
      const char *name;
      void (*func)(void);
   } methods[] = {
      {"none",          search_none         },
      {"forward",       search_forward      },
      {"backward",      search_backward     },
      {"forward-join",  search_forward_join },
      {"backward-join", search_backward_join},
      {0,               0,                  },
   };
   
   for (size_t i = 0; methods[i].name; i++)
      if (!strcmp(methods[i].name, g_config.search_mode))
         return methods[i].func;
   
   die("unkown search mode: %s", g_config.search_mode);
}

static void handle_signal(int sig)
{
   (void)sig;
   g_stop = true;
}

noreturn void search(void)
{
   void (*func)(void) = search_method();
   
   if (g_config.max_features != SIZE_MAX
       && strcmp(g_config.search_mode, "forward")
       && strcmp(g_config.search_mode, "forward-join"))
      die("--max-features can only be given when the search mode is 'forward' or 'forward-join', have '%s'",
          g_config.search_mode);
   
   if (g_config.max_links != SIZE_MAX
       && strcmp(g_config.search_mode, "forward-join")
       && strcmp(g_config.search_mode, "backward-join"))
      die("--max-links only applies when the search mode is 'forward-join' or 'backward-join', have '%s'",
          g_config.search_mode);
   
   if (g_config.max_features > g_data.num_features)
      g_config.max_features = g_data.num_features;
   
   if (g_config.compact_json) {
      compress_json(g_full_report);
      compress_json(g_step_report);
   }
   
   if (g_config.verbose)
      run_eval = eval_model_verbose;

   if (signal(SIGINT, handle_signal) == SIG_ERR ||
       signal(SIGTERM, handle_signal) == SIG_ERR)
      die("can't install signal handler: %s", strerror(errno));

   func();
   print_best();
}
