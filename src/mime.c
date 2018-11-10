#include "../include/mime.h"
#include <stdlib.h>
#include "../include/dict.h"


static dict d;


static const char *mime_map[] = {
	"html", "text/html",
	"txt" , "text/plain",
};
static const size_t mime_map_count = sizeof(mime_map) / sizeof(*mime_map) / 2;


__attribute__((constructor))
static void init()
{
	d = dict_create();
	for (int i = 0; i < mime_map_count; i++) {
		if (dict_set(d, mime_map[i*2], mime_map[i*2 + 1]) < 0)
			abort();
	}
}

__attribute__((destructor))
static void destroy()
{
	dict_free(d);
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
	return dict_get(d, lptr);
}
