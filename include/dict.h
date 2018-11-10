#ifndef DICTIONARY_H
#define DICTIONARY_H


#include <stddef.h>


#define DICT_FREE_KEYS   0x1
#define DICT_FREE_VALUES 0x2


/*
 * A dictionary is a structure optimized for mapping string keys to string
 * values. It may use any data structure internally to optimize search
 * efficiency.
 */
typedef struct dictionary {
	size_t   size;
	size_t   count;
	char   **keys;
	char   **values;
} dictionary;


/*
 * Creates a new dictionary. 
 */
int dict_create(dictionary *dict);

/*
 * Fress a dictionary.
 */
void dict_free(dictionary *dict);

/*
 * Gets a value given a key. If the key does not map to a value, NULL is returned.
 */
const char *dict_get(const dictionary *dict, const char *key);

/*
 * Sets a key that maps to a given value. if value is NULL, no mapping will be
 * added.
 */
int dict_set(dictionary *dict, const char *key, const char *value);

#endif
