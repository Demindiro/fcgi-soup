#ifndef CONTAINERS_LIST_H
#define CONTAINERS_LIST_H

#include <stddef.h>

typedef struct list {
	size_t count;
	size_t capacity;
	size_t size;
	char *_data;
} *list;


list list_create(size_t size);

void list_free(list ls);

int list_add(list ls, void *item);

int list_remove(list ls, size_t index);

int list_set(list ls, size_t index, void *item);

int list_get(list ls, size_t index, void *dest);

int list_iter(list ls, size_t *index, void *dest);

void *list_to_array(list ls);

#endif
