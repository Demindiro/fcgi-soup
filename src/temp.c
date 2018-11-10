#include "../include/temp.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "../include/dict.h"
#include "../include/list.h"
#include "util/string.h"


#define TEMPLATE_MASK       0x0F
#define TEMPLATE_MASK_TYPE  0xF0
#define TEMPLATE_SUBST      0x01
#define TEMPLATE_COND       0x02
#define TEMPLATE_COND_IFDEF 0x10
#define TEMPLATE_COND_ENDIF 0x20


static int copy_part(list parts, list lens, const char *ptr, const char *orgptr) {
	size_t l = ptr - orgptr;
	char *p = malloc(l);
	memcpy(p, orgptr, l);
	list_add(parts, &p);
	list_add(lens , &l);
	return 0;
}


template temp_create(const char *text)
{
	const char *ptr = text, *orgptr = ptr;
	template temp;

	list parts   = list_create(sizeof(*temp->parts  ));
	list lengths = list_create(sizeof(*temp->lengths));
	list args    = list_create(sizeof(*temp->args   ));
	list flags   = list_create(sizeof(*temp->flags  ));

	while (1) {
		while (*ptr != '{') {
			if (*ptr == 0) {
				if (copy_part(parts, lengths, ptr, orgptr) < 0)
					goto error;
				temp = malloc(sizeof(*temp));
				temp->parts   = list_to_array(parts  );
				temp->lengths = list_to_array(lengths);
				temp->args    = list_to_array(args   );
				temp->flags   = list_to_array(flags  );
				temp->count   = args->count;
				goto success;
			}
			ptr++;
		}
		ptr++;

		char c = *ptr;
		if (c == '{' || c == '%') {
			if (copy_part(parts, lengths, ptr - 1, orgptr) < 0)
				goto error;
			ptr++;

			while (*ptr == ' ' || *ptr == '\t')
				ptr++;
			if (*ptr == 0)
				goto error;
			orgptr = ptr;

			int f;
			char *arg = NULL;

			if (c == '{') {
				f = TEMPLATE_SUBST;
				c = '}';
			} else {
				f = TEMPLATE_COND;
				while (*ptr != ' ' && *ptr != '\t')
					ptr++;
				size_t len = ptr - orgptr;
				if (len == 5 && strncmp(orgptr, "ifdef", 5) == 0)
					f |= TEMPLATE_COND_IFDEF;
				else if (len == 5 && strncmp(orgptr, "endif", 5) == 0)
					f |= TEMPLATE_COND_ENDIF;
				else
					goto error;
				while (*ptr == ' ' || *ptr == '\t')
					ptr++;
				orgptr = ptr;
			}

			while (*ptr != c)
				ptr++;
			const char *endptr = ptr + 1;
			if (*endptr != '}')
				goto error;
			if (f & TEMPLATE_COND_ENDIF)
				goto no_arg;
			ptr--;
			while (*ptr == ' ')
				ptr--;
			ptr++;

			size_t l = ptr - orgptr;
			arg = malloc(l + 1);
			memcpy(arg, orgptr, l);
			arg[l] = 0;
no_arg:
			orgptr = ptr = endptr + 1;
			list_add(flags, &f  );
			list_add(args , &arg);
		}
	}

error:
	temp = NULL;
success:
	list_free(parts  );
	list_free(lengths);
	list_free(args   );
	list_free(flags  );
	return temp;
}


void temp_free(template temp)
{
	size_t i;
	for (i = 0; i < temp->count; i++) {
		free(temp->parts[i]);
		free(temp->args [i]);
	}
	free(temp->parts[i]);
	free(temp->parts  );
	free(temp->lengths);
	free(temp->args   );
	free(temp->flags  );
	free(temp);
}


char *temp_render(const template temp, const dict d)
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
			const char *str = dict_get(d, temp->args[i]);
			if (str == NULL)
				str = "NULL";
			if (buf_write(&buf, &offset, &bufl, str, strlen(str)) < 0)
				goto error;
		} else if (temp->flags[i] & TEMPLATE_COND) {
			if (temp->flags[i] & TEMPLATE_COND_IFDEF) {
				if (dict_get(d, temp->args[i]) == NULL) {
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
		if (buf_write(&buf, &offset, &bufl, temp->parts[i+1], temp->lengths[i+1]) < 0)
			goto error;
	}
	buf = realloc(buf, offset + 1);
	buf[offset] = 0;
	return buf;
error:
	free(buf);
	return NULL;
}
