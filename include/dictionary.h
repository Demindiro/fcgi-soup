#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <stddef.h>

#define DICT_FREE_KEYS   0x1
#define DICT_FREE_VALUES 0x2

typedef struct dictionary {
	size_t   size;
	size_t   count;
	char   **keys;
	char   **values;
} dictionary;

int dict_create(dictionary *dict);

int dict_free(dictionary *dict);

const char *dict_get(const dictionary *dict, const char *key);

int dict_set(dictionary *dict, const char *key, const char *value);

#endif
