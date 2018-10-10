#include "../include/template.h"
#include <string.h>
#include <stdlib.h>
#include "../include/dictionary.h"

static size_t grow(void *ptr, size_t len, size_t size) {
	void **buf = (void **)ptr;
	size_t nl = len * 3 / 2;
	void *tmp = realloc(*buf, nl * size);
	if (tmp == NULL)
		return -1;
	*buf = tmp;
	return nl;
}


int template_create(template *temp, const char *text)
{
	size_t size = 0x10;
	temp->parts   = malloc(size * sizeof(*temp->parts  ));
	temp->lengths = malloc(size * sizeof(*temp->lengths));
	temp->keys    = malloc(size * sizeof(*temp->keys   ));

	const char *ptr = text, *orgptr = ptr;
	for (temp->count = 0; ; temp->count++) {
		int check_bufs() {
			if (temp->count >= size) {
				if (grow(&temp->parts  , size, sizeof(*temp->parts  )) == -1 ||
				    grow(&temp->keys   , size, sizeof(*temp->keys   )) == -1)
					return -1;
				size_t s = grow(temp->lengths, size, sizeof(*temp->lengths));
				if (s == -1)
					return -1;
				size = s;
			}
			return 0;
		}
		int copy_part() {
			if (check_bufs() < 0)
				return -1;
			size_t l = ptr - orgptr;
			temp->parts[temp->count] = malloc(l);
			memcpy(temp->parts[temp->count], orgptr, l);
			temp->lengths[temp->count] = l;
			return 0;
		}

		while (*ptr != '{') {
			if (*ptr == 0) {
				if (copy_part() < 0)
					return -1;
				return 0;
			}
			ptr++;
		}

		if (copy_part() < 0)
			return -1;

		ptr++;
		orgptr = ptr;
		while (*ptr != '}') {
			if (*ptr == 0)
				goto error;
			ptr++;
		}
		size_t l = ptr - orgptr;
		temp->keys[temp->count] = malloc(l + 1);
		memcpy(temp->keys[temp->count], orgptr, l);
		temp->keys[temp->count][l] = 0;
		ptr++;
		orgptr = ptr;
	}

error:
	template_free(temp);
	return -1;
}


void template_free(template *temp)
{
	for (size_t i = 0; i < temp->count; i++) {
		free(temp->parts[i]);
		free(temp->keys [i]);
	}
	free(temp->parts  );
	free(temp->lengths);
	free(temp->keys   );
}


char *template_parse(const template *temp, const dictionary *dict)
{
	if (temp->parts == NULL)
		return NULL;
	size_t bufl = 0x10000;
	size_t strl = temp->lengths[0];
	if (strl > bufl)
		bufl = strl;
	char  *buf  = malloc(bufl);
	memcpy(buf, temp->parts[0], strl);
	char *ptr = buf + strl;

	for (size_t i = 0; i < temp->count; i++) {
		const char *str = dict_get(dict, temp->keys[i]);
		size_t strl;
		if (str == NULL)
			str = "NULL";
		strl = strlen(str);
		if (bufl < (ptr - buf) + strl) {
			size_t nbufl = bufl + (ptr - buf) + strl;
			char *tmp = realloc(buf, nbufl);
			if (tmp == NULL) {
				free(buf);
				return NULL;
			}
			buf  = tmp;
			ptr += tmp - buf;
			bufl = nbufl;
		}
		memcpy(ptr, str, strl);
		ptr += strl;
		memcpy(ptr, temp->parts[i+1], temp->lengths[i+1]);
		ptr += temp->lengths[i+1];
	}

	return buf;
}
