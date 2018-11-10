#include "../include/dict.h"
#include <stdlib.h>
#include <string.h>

static size_t get_index(const dict d, const char *key)
{
	if (key == NULL)
		return -1;
	for (size_t i = 0; i < d->count; i++) {
		if (strcmp(key, d->keys[i]) == 0)
			return i;
	}
	return -1;
}


dict dict_create() {
	dict d = malloc(sizeof(struct dict));
	d->size    = 16;
	d->keys    = malloc(d->size * sizeof(char *));
	if (d->keys   == NULL)
		goto error;
	d->values  = malloc(d->size * sizeof(char *));
	if (d->values == NULL) {
		free(d->keys);
		goto error;
	}
	d->count   = 0;
	return d;
error:
	free(d);
	return NULL;
}


void dict_free(dict d)
{
	for (size_t i = 0; i < d->count; i++)
		free(d->keys[i]);
	for (size_t i = 0; i < d->count; i++)
		free(d->values[i]);
	free(d->keys);
	free(d->values);
	free(d);
}


const char *dict_get(const dict d, const char *key)
{
	size_t i = get_index(d, key);
	if (i == -1)
		return NULL;
	return d->values[i];
}


int dict_set(dict d, const char *key, const char *value)
{
	if (key == NULL)
		return -1;
	size_t i = get_index(d, key);
	if (i == -1) {
		if (value == NULL)
			return 0;
		i = d->count;
		if (i >= d->size) {
			size_t s = d->size * 3 / 2;
			char **tmp = realloc(d->keys, s * sizeof(char *));
			if (tmp == NULL)
				return -1;
			d->keys = tmp;
			tmp = realloc(d->values, s * sizeof(char *));
			if (tmp == NULL)
				return -1;
			d->values = tmp;
			d->size = s;
		}

		size_t l = strlen(key);
		d->keys[i] = malloc(l + 1);
		if (d->keys[i] == NULL)
			return -1;
		memcpy(d->keys[i], key, l);
		d->keys[i][l] = 0;
		d->count++;
	} else {
		free(d->values[i]);
	}
	if (value == NULL) {
		d->count--;
		memmove(d->values + (i - 1), d->values + i, (d->count - i) * sizeof(char *));
		memmove(d->keys   + (i - 1), d->values + i, (d->count - i) * sizeof(char *));
	} else {
		size_t l = strlen(value);
		d->values[i] = malloc(l + 1);
		if (d->values[i] == NULL)
			return -1;
		memcpy(d->values[i], value, l);
		d->values[i][l] = 0;
	}
	return 0;
}
