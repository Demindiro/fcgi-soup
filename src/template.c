#include "../include/template.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "../include/dictionary.h"
#include "util/string.h"


#define TEMPLATE_MASK       0x0F
#define TEMPLATE_MASK_TYPE  0xF0
#define TEMPLATE_SUBST      0x01
#define TEMPLATE_COND       0x02
#define TEMPLATE_COND_IFDEF 0x10
#define TEMPLATE_COND_ENDIF 0x20


static size_t grow(void *ptr, size_t len, size_t size) {
	void **buf = (void **)ptr;
	size_t nl = len * 3 / 2;
	void *tmp = realloc(*buf, nl * size);
	if (tmp == NULL)
		return -1;
	*buf = tmp;
	return nl;
}


static int check_bufs(template *temp, size_t *size) {
	if (temp->count >= *size) {
		if (grow(&temp->parts, *size, sizeof(*temp->parts)) == -1 ||
		    grow(&temp->args , *size, sizeof(*temp->args )) == -1 ||
		    grow(&temp->flags, *size, sizeof(*temp->flags)) == -1)
			return -1;
		size_t s = grow(&temp->lengths, *size, sizeof(*temp->lengths));
		if (s == -1)
			return -1;
		*size = s;
	}
	return 0;
}


static int copy_part(template *temp, const char *ptr, const char *orgptr, size_t *size) {
	if (check_bufs(temp, size) < 0)
		return -1;
	size_t l = ptr - orgptr;
	temp->parts[temp->count] = malloc(l);
	memcpy(temp->parts[temp->count], orgptr, l);
	temp->lengths[temp->count] = l;
	return 0;
}


int template_create(template *temp, const char *text)
{
	size_t size   = 0x10;
	temp->parts   = malloc(size * sizeof(*temp->parts  ));
	temp->lengths = malloc(size * sizeof(*temp->lengths));
	temp->args    = malloc(size * sizeof(*temp->args   ));
	temp->flags   = malloc(size * sizeof(*temp->flags  ));
	temp->count   = 0;

	const char *ptr = text, *orgptr = ptr;
	while (1) {
		while (*ptr != '{') {
			if (*ptr == 0) {
				if (copy_part(temp, ptr, orgptr, &size) < 0)
					goto error;
				return 0;
			}
			ptr++;
		}
		ptr++;

		char c = *ptr;
		if (c == '{' || c == '%') {
			if (copy_part(temp, ptr - 1, orgptr, &size) < 0)
				goto error;
			ptr++;

			while (*ptr == ' ' || *ptr == '\t')
				ptr++;
			if (*ptr == 0)
				goto error;
			orgptr = ptr;

			if (c == '{') {
				temp->flags[temp->count] = TEMPLATE_SUBST;
				c = '}';
			} else {
				temp->flags[temp->count] = TEMPLATE_COND;
				while (*ptr != ' ' && *ptr != '\t')
					ptr++;
				size_t len = ptr - orgptr;
				if (len == 5 && strncmp(orgptr, "ifdef", 5) == 0) {
					temp->flags[temp->count] |= TEMPLATE_COND_IFDEF;
				} else if (len == 5 && strncmp(orgptr, "endif", 5) == 0) {
					temp->flags[temp->count] |= TEMPLATE_COND_ENDIF;
				} else {
					goto error;
				}
				while (*ptr == ' ' || *ptr == '\t')
					ptr++;
				orgptr = ptr;
			}

			while (*ptr != c)
				ptr++;
			const char *endptr = ptr + 1;
			if (*endptr != '}')
				goto error;
			if (temp->flags[temp->count] & TEMPLATE_COND_ENDIF)
				goto no_arg;
			ptr--;
			while (*ptr == ' ')
				ptr--;
			ptr++;

			size_t l = ptr - orgptr;
			temp->args[temp->count] = malloc(l + 1);
			memcpy(temp->args[temp->count], orgptr, l);
			temp->args[temp->count][l] = 0;

		no_arg:
			orgptr = ptr = endptr + 1;
			temp->count++;
		}
	}

error:
	template_free(temp);
	return -1;
}


void template_free(template *temp)
{
	for (size_t i = 0; i < temp->count; i++) {
		free(temp->parts[i]);
		free(temp->args [i]);
	}
	free(temp->parts  );
	free(temp->lengths);
	free(temp->args   );
}


char *template_parse(const template *temp, const dictionary *dict)
{
	if (temp->parts == NULL)
		return NULL;

	size_t bufl = 0x100;
	size_t strl = temp->lengths[0];
	if (strl >= bufl)
		bufl = strl + 1;
	char  *buf  = malloc(bufl);
	memcpy(buf, temp->parts[0], strl);
	size_t offset = strl;

	for (size_t i = 0; i < temp->count; i++) {	
		if (temp->flags[i] & TEMPLATE_SUBST) {
			const char *str = dict_get(dict, temp->args[i]);
			if (str == NULL)
				str = "NULL";
			strl = strlen(str);
			if (bufl < offset + strl) {
				size_t nbufl = (offset + strl + temp->lengths[i+1] + 1) * 3 / 2;
				char *tmp = realloc(buf, nbufl);
				if (tmp == NULL)
					goto error;
				buf  = tmp;
				bufl = nbufl;
			}
			memcpy(&buf[offset], str, strl);
			offset += strl;
		} else if (temp->flags[i] & TEMPLATE_COND) {
			if (temp->flags[i] & TEMPLATE_COND_IFDEF) {
				if (dict_get(dict, temp->args[i]) == NULL) {
					int c = 0;
					while (c >= 0) {
						i++;
						if (temp->flags[i] & TEMPLATE_COND_ENDIF)
							c--;
						else if (temp->flags[i] & TEMPLATE_COND)
							c++;
					}
				}
			} else if (temp->flags[i] & TEMPLATE_COND_ENDIF) {
				/* NOOP */
			} else {
				errno = EINVAL;
				goto error;
			}
		} else {
			errno = EINVAL;
			goto error;
		}
		memcpy(&buf[offset], temp->parts[i+1], temp->lengths[i+1]);
		offset += temp->lengths[i+1];
	}
	buf = realloc(buf, offset + 1);
	buf[offset] = 0;
	return buf;
error:
	free(buf);
	return NULL;
}
