#include <stdio.h>
#include <string.h>
#include "common.h"

static const char no_mem[] = "out of memory";

void *xmalloc(size_t size)
{
   void *mem = malloc(size);
   if (!mem)
      die(no_mem);
   return mem;
}

void *xcalloc(size_t nmemb, size_t size)
{
   void *mem = calloc(nmemb, size);
   if (!mem)
      die(no_mem);
   return mem;
}

void *xrealloc(void *mem, size_t size)
{
   mem = realloc(mem, size);
   if (!mem)
      die(no_mem);
   return mem;
}

char *xstrdup(const char *str)
{
   size_t size = strlen(str) + 1;
   return memcpy(xmalloc(size), str, size);
}

noreturn void die(const char *format, ...)
{
   extern const char *g_progname;
   
   fprintf(stderr, "%s: ", g_progname);
   
   va_list ap;
   va_start(ap, format);
   vfprintf(stderr, format, ap);
   va_end(ap);
   
   putc('\n', stderr);

   exit(EXIT_FAILURE);
}
