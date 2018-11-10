#ifndef CONTAINERS_LIST_H
#define CONTAINERS_LIST_H

#include <stddef.h>

typedef struct list {
	size_t count;
	size_t capacity;
	size_t size;
	char *_data;
} list;


int list_create(list *ls, size_t size);

void list_free(list *ls);

int list_add(list *ls, void *item);

int list_remove(list *ls, size_t index);

int list_set(list *ls, size_t index, void *item);

const void *list_get(list *ls, size_t index);

#endif
