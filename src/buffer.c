#include <string.h>
#include "buffer.h"
#include "common.h"

void buffer_ensure(struct buffer *buf, size_t size)
{
   ENLARGE(buf->data, size + 1, buf->alloc, 16);
}

void buffer_cat(struct buffer *buf, const void *data, size_t size)
{
   buffer_ensure(buf, buf->size + size);
   memcpy(&buf->data[buf->size], data, size);
   buf->data[buf->size += size] = '\0';
}

void buffer_set_json(struct buffer *buf, const char *str)
{
   buf->size = 0;
   buffer_ensure(buf, strlen(str) * 2 + 2);
   char *data = buf->data;
   
   *data++ = '"';
   for (size_t i = 0; str[i]; i++) {
      unsigned char c = str[i];
      if (c == '"' || c == '\\' || c <= 0x1f)
         *data++ = '\\';
      *data++ = c;
   }
   *data++ = '"';
   *data = '\0';
   
   buf->size = data - buf->data;
}
