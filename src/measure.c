#include <string.h>
#include "measure.h"
#include "common.h"
#include "dataset.h"
#include "eval.h"
#include "cmd.h"

extern struct config g_config;

static double accuracy_micro(const struct conf_mat *mat)
{
   double div = mat->true_pos + mat->false_pos + mat->true_neg + mat->false_neg;
   if (div)
      return (mat->true_pos + mat->true_neg) / div;
   return 0.;
}

static double precision_micro(const struct conf_mat *mat)
{
   double div = mat->true_pos + mat->false_pos;
   if (div)
      return mat->true_pos / div;
   return 0.;
}

static double recall_micro(const struct conf_mat *mat)
{
   double div = mat->true_pos + mat->false_neg;
   if (div)
      return mat->true_pos / div;
   return 0.;
}

static double F1_micro(const struct conf_mat *mat)
{
   double prec = precision_micro(mat);
   double rec = recall_micro(mat);
   
   if (prec || rec)
      return 2 * prec * rec / (prec + rec);
   return 0.;
}

#define _(NAME)                                                                \
static double NAME##_macro(const struct conf_mat *mat)                         \
{                                                                              \
   double sum = 0;                                                             \
   size_t num_labels = g_data.num_labels;                                      \
                                                                               \
   for (size_t label = 0; label < num_labels; label++)                         \
      sum += NAME##_micro(&mat[label]);                                        \
                                                                               \
   return sum / (double)num_labels;                                            \
}
_(accuracy)
_(precision)
_(recall)
_(F1)
#undef _

#define _(TYPE)                                                                \
static void full_eval_##TYPE(struct measures *ret, const struct conf_mat *mat) \
{                                                                              \
   *ret = (struct measures){                                                   \
      .accuracy = accuracy_##TYPE(mat),                                        \
      .precision = precision_##TYPE(mat),                                      \
      .recall = recall_##TYPE(mat),                                            \
      .F1 = F1_##TYPE(mat)                                                     \
   };                                                                          \
}
_(micro)
_(macro)
#undef _

static double (*get_measure_func(bool macro))(const struct conf_mat *)
{   
   const struct {
      const char *name;
      double (*micro)(const struct conf_mat *);
      double (*macro)(const struct conf_mat *);
   } funcs[] = {
   #define _(NAME) {#NAME, NAME##_micro, NAME##_macro},
      _(accuracy)
      _(precision)
      _(recall)
      _(F1)
   #undef _
   };

   for (size_t i = 0; i < sizeof funcs / sizeof *funcs; i++)
      if (!strcmp(g_config.measure_name, funcs[i].name))
         return macro ? funcs[i].macro : funcs[i].micro;

   die("invalid measure: %s", g_config.measure_name);
}

double (*measure_func)(const struct conf_mat *);
void (*full_eval)(struct measures *, const struct conf_mat *);

void measure_init(void)
{
   bool macro = !strcmp(g_config.classification_mode, "multiclass") &&
                !strcmp(g_config.averaging_mode, "macro");
   
   measure_func = get_measure_func(macro);
   full_eval = macro ? full_eval_macro : full_eval_micro;
}
