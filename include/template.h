#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <stddef.h>
#include "dictionary.h"

typedef struct template {
	size_t   count;
	char   **parts;
	size_t  *lengths;
	char   **keys;
} template;


int template_create(template *temp, const char *text);

void template_free(template *temp);

char *template_parse(const template *temp, const dictionary *dict);

#endif
