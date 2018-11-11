#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>

typedef struct array {
	const size_t len;
	const size_t size;
	char *const  data;
} *array;

array array_create(size_t size, size_t len);

array array_from(char *data, size_t size, size_t len);

void array_free(array a);

int array_set(array a, size_t i, void *src);

int array_get(array a, size_t i, void *dest);

#endif
