#include "../include/dict.h"
#include <stdlib.h>
#include <string.h>

static size_t get_index(const dictionary *dict, const char *key)
{
	if (key == NULL)
		return -1;
	for (size_t i = 0; i < dict->count; i++) {
		if (strcmp(key, dict->keys[i]) == 0)
			return i;
	}
	return -1;
}

int dict_create(dictionary *dict) {
	dict->size    = 16;
	dict->keys    = malloc(dict->size * sizeof(char *));
	if (dict->keys   == NULL)
		return -1;
	dict->values  = malloc(dict->size * sizeof(char *));
	if (dict->values == NULL) {
		free(dict->keys);
		return -1;
	}
	dict->count   = 0;
	return 0;
}

void dict_free(dictionary *dict)
{
	for (size_t i = 0; i < dict->count; i++)
		free(dict->keys[i]);
	for (size_t i = 0; i < dict->count; i++)
		free(dict->values[i]);
	free(dict->keys);
	free(dict->values);
}

const char *dict_get(const dictionary *dict, const char *key)
{
	size_t i = get_index(dict, key);
	if (i == -1)
		return NULL;
	return dict->values[i];
}

int dict_set(dictionary *dict, const char *key, const char *value)
{
	if (key == NULL)
		return -1;
	size_t i = get_index(dict, key);
	if (i == -1) {
		if (value == NULL)
			return 0;
		i = dict->count;
		if (i >= dict->size) {
			size_t s = dict->size * 3 / 2;
			char **tmp = realloc(dict->keys, s * sizeof(char *));
			if (tmp == NULL)
				return -1;
			dict->keys = tmp;
			tmp = realloc(dict->values, s * sizeof(char *));
			if (tmp == NULL)
				return -1;
			dict->values = tmp;
			dict->size = s;
		}

		size_t l = strlen(key);
		dict->keys[i] = malloc(l + 1);
		if (dict->keys[i] == NULL)
			return -1;
		memcpy(dict->keys[i], key, l);
		dict->keys[i][l] = 0;
		dict->count++;
	} else {
		free(dict->values[i]);
	}
	if (value == NULL) {
		dict->count--;
		memmove(dict->values + (i - 1), dict->values + i, (dict->count - i) * sizeof(char *));
		memmove(dict->keys   + (i - 1), dict->values + i, (dict->count - i) * sizeof(char *));
	} else {
		size_t l = strlen(value);
		dict->values[i] = malloc(l + 1);
		if (dict->values[i] == NULL)
			return -1;
		memcpy(dict->values[i], value, l);
		dict->values[i][l] = 0;
	}
	return 0;
}
