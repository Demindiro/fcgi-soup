#include "../include/array.h"
#include <stdlib.h>
#include <string.h>


array array_create(size_t size, size_t len)
{
	array a = malloc(sizeof(*a));
	*(size_t *)&a->size = size;
	*(size_t *)&a->len  = len;
	*(char  **)&a->data = calloc(size, len);
	return a;
}


array array_from(char *data, size_t size, size_t len)
{
	array a = malloc(sizeof(*a));
	*(size_t *)&a->size = size;
	*(size_t *)&a->len  = len;
	*(char  **)&a->data = data;
	return a;
}


void array_free(array a)
{
	free(a->data);
	free(a);
}


int array_set(array a, size_t i, void *src)
{
	if (i >= a->len)
		return -1;
	memcpy((a->data + (i * a->size)), src, a->size);
	return 0;
}


int array_get(array a, size_t i, void *dest)
{
	if (i >= a->len)
		return -1;
	memcpy(dest, a->data + (i * a->size), a->size);
	return 0;
}
