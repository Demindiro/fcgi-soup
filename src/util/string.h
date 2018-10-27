#ifndef STRING_H
#define STRING_H

int buf_write(void *pbuf, size_t *index, size_t *size, const void *src, size_t count);

char *string_copy(const char *src);

#endif
