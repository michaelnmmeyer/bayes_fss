#ifndef BFSS_COMMON_H
#define BFSS_COMMON_H

#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdnoreturn.h>
#include <assert.h>

void *xmalloc(size_t size);

void *xcalloc(size_t nmemb, size_t size);

void *xrealloc(void *mem, size_t size);

char *xstrdup(const char *str);

noreturn void die(const char *format, ...);

#define ENLARGE(buf, size, alloc, init) do {                                   \
   const size_t size_ = (size);                                                \
   assert(size_ > 0);                                                          \
   assert(buf == buf && alloc == alloc && init == init);                       \
   if (size_ > alloc) {                                                        \
      if (!alloc) {                                                            \
         alloc = size_ > init ? size_ : init;                                  \
         buf = xmalloc(alloc * sizeof *buf);                                   \
      } else {                                                                 \
         assert(buf);                                                          \
         size_t tmp_ = alloc + (alloc >> 1);                                   \
         alloc = tmp_ > size_ ? tmp_ : size_;                                  \
         buf = xrealloc(buf, alloc * sizeof *buf);                             \
      }                                                                        \
   }                                                                           \
} while (0)

#endif
