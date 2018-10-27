#ifndef STRING_H
#define STRING_H

#include <string.h>

static int buf_write(void *pbuf, size_t *index, size_t *size, const void *src, size_t count) {
	void **buf = pbuf;
	if (*size < *index + count) {
		size_t ns = (*index + count) * 3 / 2
		void *tmp = realloc(*buf, ns);
		if (tmp == NULL)
			return -1;
		*buf = tmp;
		*size = ns;
	}
	memcpy(*buf + *index, src, count);
	*index += count;
	return 0;
} 

static char *string_copy(const char *src) {
	size_t l = strlen(src);
	char *cp = malloc(l + 1);
	if (cp == NULL)
		return NULL;
	memcpy(cp, src, l);
	cp[l] = 0;
	return cp;
}


#endif
