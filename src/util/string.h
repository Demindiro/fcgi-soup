#ifndef STRING_H
#define STRING_H

#include <string.h>

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
