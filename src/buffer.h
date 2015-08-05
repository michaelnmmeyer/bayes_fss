#ifndef BFSS_BUFFER_H
#define BFSS_BUFFER_H

#include <stddef.h>
#include <stdlib.h>

struct buffer {
   char *data;
   size_t size, alloc;
};

#define BUFFER_INIT {.data = ""}

void buffer_ensure(struct buffer *buf, size_t size);

static inline void buffer_fini(struct buffer *buf)
{
   if (buf->alloc)
      free(buf->data);
}

static inline void buffer_clear(struct buffer *buf)
{
   if (buf->alloc)
      buf->data[buf->size = 0] = '\0';
}

void buffer_set_json(struct buffer *buf, const char *str);

void buffer_cat(struct buffer *buf, const void *data, size_t size);

static inline void buffer_catc(struct buffer *buf, int c)
{
   buffer_ensure(buf, buf->size + 1);
   buf->data[buf->size++] = c;
   buf->data[buf->size] = '\0';
}

#endif
