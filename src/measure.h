#ifndef BFSS_MEASURE_H
#define BFSS_MEASURE_H

struct conf_mat {
   unsigned long true_pos;
   unsigned long true_neg;
   unsigned long false_pos;
   unsigned long false_neg;
};

struct measures {
   double accuracy;
   double precision;
   double recall;
   double F1;
};

double (*measure_func)(const struct conf_mat *);
void (*full_eval)(struct measures *, const struct conf_mat *);

void measure_init(void);

#endif
