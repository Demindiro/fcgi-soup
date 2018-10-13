#include "../include/mime.h"
#include <stdlib.h>
#include "../include/dictionary.h"

static dictionary dict;


static const char *mime_map[] = {
	"html", "text/html",
	"txt" , "text/plain",
};
static const size_t mime_map_count = sizeof(mime_map) / sizeof(*mime_map) / 2;


__attribute__((constructor))
static void init()
{
	dict_create(&dict);
	for (int i = 0; i < mime_map_count; i++) {
		if (dict_set(&dict, mime_map[i*2], mime_map[i*2 + 1]) < 0)
			abort();
	}
}

const char *get_mime_type(const char *filename)
{
	const char *ptr = filename, *lptr = NULL;
	while (*ptr != 0) {
		if (*ptr == '.')
			lptr = ptr + 1;
		ptr++;
	}
	if (lptr == NULL || *lptr == 0)
		return NULL;
	const char *type = dict_get(&dict, lptr);
	return type;
}
