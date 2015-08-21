#include <stdio.h>
#include <string.h>
#include <stdnoreturn.h>
#include "dataset.h"
#include "common.h"
#include "column.h"
#include "eval.h"
#include "search.h"
#include "cmd.h"

#define VERSION "0.3"

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

noreturn static void version(void)
{
   const char msg[] =
   "Bayes FSS version "VERSION"\n"
   "Copyright (c) 2015 Michaël Meyer"
   ;
   puts(msg);
   exit(EXIT_SUCCESS);
}

static void check_options(int argc)
{
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
   
   if (argc > 1)
      die("excess arguments");
   if (!argc)
      die("no dataset specified");
}

int main(int argc, char **argv)
{
   struct option options[] = {
      {'k',  "folds",          OPT_SIZE_T(g_config.num_folds)            },
      {'S',  "smooth",         OPT_DOUBLE(g_config.smooth)               },
      {'m',  "mode",           OPT_STR(g_config.classification_mode)     },
      {'t',  "truth",          OPT_STR(g_config.positive_label_name)     },
      {'a',  "average",        OPT_STR(g_config.averaging_mode)          },
      {'M',  "measure",        OPT_STR(g_config.measure_name)            },
      {'s',  "search",         OPT_STR(g_config.search_mode)             },
      {'L',  "max-links",      OPT_SIZE_T(g_config.max_links)            },
      {'F',  "max-features",   OPT_SIZE_T(g_config.max_features)         },
      {'v',  "verbose",        OPT_BOOL(g_config.verbose)                },
      {'c',  "compact",        OPT_BOOL(g_config.compact_json)           }, 
      {'\0', "version",        OPT_FUNC(version)                         },
      {'\0', 0,                .z = 0                                    },
   };
   const char help[] =
      #include "help_screen.h"
   ;

   // Special case.
   if (argc == 1) {
      fprintf(stderr, help, g_progname);
      return EXIT_FAILURE;
   }
   
   parse_options(options, help, &argc, &argv);
   g_config.dataset_path = *argv;
   check_options(argc);
   
   load_dataset();
   eval_init();
   measure_init();
   search();
}
