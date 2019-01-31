#include "../include/mime.h"
#include <stdlib.h>
#include "dict.h"
#include "temp-cstring.h"


static cinja_dict d;


static const char *mime_map[] = {
	"html", "text/html",
	"txt" , "text/plain",
};
static const size_t mime_map_count = sizeof(mime_map) / sizeof(*mime_map) / 2;


__attribute__((constructor))
static void init()
{
	d = cinja_dict_create();
	for (int i = 0; i < mime_map_count; i++) {
		if (cinja_dict_set(d, string_create(mime_map[i*2]), string_create(mime_map[i*2 + 1])) < 0)
			abort();
	}
}


const string get_mime_type(const string filename)
{
	const char *ptr = filename->buf, *lptr = NULL;
	while (*ptr != 0) {
		if (*ptr == '.')
			lptr = ptr + 1;
		ptr++;
	}
	if (lptr == NULL || *lptr == 0)
		return NULL;
	string s = temp_string_create(lptr);
	string r = cinja_dict_get(d, s).value;
	return r;
}
