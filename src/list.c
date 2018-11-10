#include "../include/list.h"
#include <stdlib.h>
#include <string.h>

int list_create(list *ls, size_t size)
{
	ls->size = size;
	ls->capacity = 2;
	ls->_data = malloc(ls->capacity * ls->size);
	if (ls->_data == NULL)
		return -1;
	ls->count = 0;
	return 0;
}


void list_free(list *ls)
{
	free(ls->_data);
}


int list_add(list *ls, void *item)
{
	if (ls->capacity == ls->count) {
		size_t c = ls->capacity * 3 / 2;
		void *tmp = realloc(ls->_data, c * ls->size);
		if (tmp == NULL)
			return -1;
		ls->_data = tmp;
		ls->capacity = c;
	}
	memcpy(ls->_data + (ls->count * ls->size), item, ls->size);
	ls->count++;
	return 0;
}


int list_remove(list *ls, size_t index)
{
	if (ls->count <= index)
		return -1;
	ls->count--;
	memmove(ls->_data + (index * ls->size),
	        ls->_data + ((index + 1) * ls->size),
	        (ls->count - index) * ls->size);
	return 0;
}


int list_set(list *ls, size_t index, void *item)
{
	if (ls->count <= index)
		return -1;
	memcpy(ls->_data + (index * ls->size), item, ls->size);
	return 0;
}


const void *list_get(list *ls, size_t index)
{
	if (ls->count <= index)
		return NULL;
	return ls->_data + (index * ls->size);
}
