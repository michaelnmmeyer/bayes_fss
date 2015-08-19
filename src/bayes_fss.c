#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdnoreturn.h>

#include "dataset.h"
#include "common.h"
#include "column.h"
#include "eval.h"
#include "search.h"

#define VERSION "0.2"

const char *g_progname = "bayes_fss";

struct config g_config = {
   .num_folds = 5,
   .smooth = 0.5,
   .measure_name = "F1",
   .search_mode = "forward-join",
   .averaging_mode = "macro",
   .classification_mode = "multiclass",
   .max_links = SIZE_MAX,
   .max_features = SIZE_MAX,
   .verbose = false,
   .compact_json = false,
};

static void display_help(FILE *fp)
{
   const char help[] =
   #include "help_screen.h"
   ;
   fprintf(fp, help, g_progname);
}

noreturn static void usage(void)
{
   display_help(stdout);
   exit(EXIT_SUCCESS);
}

noreturn static void version(void)
{
   const char msg[] =
   "Bayes FSS version "VERSION"\n"
   "Copyright (c) 2015 Michaël Meyer"
   ;
   puts(msg);
   exit(EXIT_SUCCESS);
}

enum option_type {
   OPT_SIZE_T,
   OPT_DOUBLE,
   OPT_STRING,
   OPT_BOOL,
   OPT_FUNC,
   
   NUM_TYPES
};

static const bool no_args[NUM_TYPES] = {
   [OPT_BOOL] = true,
   [OPT_FUNC] = true,
};

static struct option {
   const char *name;
   enum option_type type;
   union {
      size_t *z;
      double *d;
      const char **s;
      void (*f)(void);
      bool *b;
   };
} g_options[] = {
   {"--folds",        OPT_SIZE_T, .z = &g_config.num_folds          },
   {"--smooth",       OPT_DOUBLE, .d = &g_config.smooth             },
   {"--mode",         OPT_STRING, .s = &g_config.classification_mode},
   {"--truth",        OPT_STRING, .s = &g_config.positive_label_name},
   {"--average",      OPT_STRING, .s = &g_config.averaging_mode     },
   {"--measure",      OPT_STRING, .s = &g_config.measure_name       },
   {"--search",       OPT_STRING, .s = &g_config.search_mode        },
   {"--max-links",    OPT_SIZE_T, .z = &g_config.max_links          },
   {"--max-features", OPT_SIZE_T, .z = &g_config.max_features       },
   {"-v",             OPT_BOOL,   .b = &g_config.verbose            },
   {"--verbose",      OPT_BOOL,   .b = &g_config.verbose            }, 
   {"--compact",      OPT_BOOL,   .b = &g_config.compact_json       }, 
   {"-h",             OPT_FUNC,   .f = usage                        },
   {"--help",         OPT_FUNC,   .f = usage                        },
   {"--version",      OPT_FUNC,   .f = version                      },
   {0,                0,          .z = 0,                           },
};

static void get_option(struct option *opt, const char *val)
{
   bool err = false;
   
   switch (opt->type) {
   case OPT_STRING:
      if (*val == '\0')
         err = true;
      else
         *opt->s = val;
      break;
   case OPT_SIZE_T:
      if (!strcmp(val, "inf"))
         *opt->z = SIZE_MAX;
      else if (sscanf(val, "%zu", opt->z) != 1)
         err = true;
      break;
   case OPT_DOUBLE:
      if (sscanf(val, "%lf", opt->d) != 1)
         err = true;
      break;
   case OPT_BOOL:
      *opt->b = true;
      break;
   case OPT_FUNC:
      opt->f();
      break;
   default:
      die("unexpected error");
   }
   
   if (err)
      die("invalid argument '%s' for %s", val, opt->name);
}

static void get_options(int argc, char **argv)
{
   int i;
   for (i = 1; i < argc && argv[i][0] == '-'; i++) {
      if (!strcmp(argv[i], "--")) {
         i++;
         break;
      }
      char *val = argv[i][1] == '-' ? strchr(argv[i], '=') : NULL;
      if (val)
         *val++ = '\0';
      struct option *opt = g_options;
      while (opt->name && strcmp(opt->name, argv[i]))
         opt++;
      if (!opt->name)
         die("unknown option '%s'", argv[i]);
      if (!no_args[opt->type] && !val)
         die("option %s requires an argument", argv[i]);
      get_option(opt, val);
   }
   
   if (g_config.num_folds < 2)
      die("--num-folds must be >= 2");
   if (g_config.smooth <= 0.)
      die("--smooth must be > 0.0");
   
   if (!strcmp(g_config.classification_mode, "binary")) {
      if (!g_config.positive_label_name)
         die("--truth must be given for binary classification");
   } else if (!strcmp(g_config.classification_mode, "multiclass")) {
      if (g_config.positive_label_name)
         die("--truth doesn't make sense for multiclass classification");
   }
   
   if (argc - i > 1)
      die("excess arguments");
   if (argc - i == 0) {
      display_help(stderr);
      exit(EXIT_FAILURE);
   }
   g_config.dataset_path = argv[i];
}

static void set_progname(const char *name)
{
   const char *slash = strrchr(name, '/');
   if (slash && slash[1])
      g_progname = slash + 1;
}

int main(int argc, char **argv)
{
   set_progname(*argv);
   get_options(argc, argv);
   
   load_dataset();
   eval_init();
   measure_init();
   search();
}
