#include "../include/art.h"
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "util/string.h"
#include "../include/dict.h"
#include "../include/temp.h"


/*
 * Helpers
 */
static struct date parse_date(const char *str)
{
	struct date date;
	uint32_t M, d, h, m;
	sscanf(str, "%d-%d-%d %d:%d", &date.year, &M, &d, &h, &m);
	date.month = M;
	date.day   = d;
	date.hour  = h;
	date.min   = m;
	return date;
}


/*
 * Comments
 */

static comment parse_comment(char *ptr, size_t len)
{
	comment c = malloc(sizeof(*c));
	char *p = ptr, *s = ptr;
	while (*ptr != '\n')
		ptr++;
	*ptr = 0;
	c->author = string_copy(p);
	ptr++;

	p = ptr;
	while (*ptr != '\n')
		ptr++;
	*(ptr++) = 0;
	c->date = parse_date(p);

	p = ptr;
	while (*ptr != '\n')
		ptr++;
	*(ptr++) = 0;
	sscanf(p, "%d", &c->reply_to);

	c->body = malloc(len - (ptr - s) + 1);
	memcpy(c->body, ptr, len - (ptr - s));
	c->body[len - (ptr - s)] = 0;

	c->replies = list_create(sizeof(c));

	return c;
}


list art_get_comments(art_root root, const char *name)
{
	char file[256], *ptr = file;
	size_t l = strlen(root->dir);
	const char **entries = NULL;
	comment *comments = NULL;
	list ls = NULL;
 
	memcpy(ptr, root->dir, l);
	ptr += l;
	l = sizeof("comments");
	memcpy(ptr, "comments/", l);
	ptr += l;
	l = strlen(name);
	memcpy(ptr, name, l);
	ptr += l;
	*ptr = 0;

	FILE *f = fopen(file, "r");
	fseek(f, 0, SEEK_END);
	size_t s = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = malloc(s + 1);
	fread(buf, 1, s, f);
	buf[s] = 0;
	fclose(f);

	list cs = list_create(sizeof(comment));
	ptr = buf;
	for (int id = 0; ptr - buf < s; id++) {
		while (*ptr == '\n')
			ptr++;
		char *p = ptr;
		while (1) {
			if (*ptr == '\n') {
				ptr++;
				if (*ptr == '\n') {
					ptr++;
					if (*ptr == '\n')
						break;
				}
			}
			if (ptr - buf >= s)
				break;
			ptr++;
		}
		if (ptr - buf >= s)
			break;
		comment c = parse_comment(p, ptr - p - 1);
		c->id = id;
		list_add(cs, &c);
		ptr++;
	}
	free(buf);

	ls = list_create(sizeof(comment));
	comment c;
	size_t i = 0;
	while (list_iter(cs, &i, &c)) {
		list l;
		if (c->reply_to == -1) {
			l = ls;
		} else {
			comment d;
			list_get(cs, c->reply_to, &d);
			l = d->replies;
		}
		if (list_add(l, &c) < 0)
			goto error;
	}

	goto success;
error:
	if (ls != NULL)
		list_free(ls);
	ls = NULL;
success:
	free(entries);
	free(comments);
	return ls;
}


void art_free_comments(list ls)
{
	for (size_t i = 0; i < ls->count; i++) {
		comment c;
		list_get(ls, i, &c);
		free(c->author);
		free(c->body);
		free(c);
		art_free_comments(c->replies);
	}
	list_free(ls);
}


int art_add_comment(art_root root, const char *uri, comment c, size_t reply_to)
{
	article a;
	size_t i = 0;
	while (list_iter(root->articles, &i, &a)) {
		if (strcmp(a->uri, uri) == 0)
			goto found;
	}
	return -1;

found:;
	char buf[256], *ptr = buf;
	size_t l = strlen(root->dir);
	memcpy(ptr, root->dir, l);
	ptr += l;
	l = sizeof("comments");
	memcpy(ptr,"comments/", l);
	ptr += l;
	l = strlen(uri);
	memcpy(ptr, uri, l);
	ptr += l;
	*ptr = 0;

	FILE *f = fopen(buf, "a");
	struct date d = c->date;

	fprintf(f,
	        "%s\n"
	        "%u-%u-%u %u:%u\n"
	        "%ld\n",
		c->author,
	        d.year, d.month, d.day, d.hour, d.min,
		reply_to);
	ptr = c->body;
	int nc = 0;
	while (*ptr != 0) {
		switch (*ptr) {
		default : fputc(*ptr  , f); nc = 0; break;
		case '<': fputs("&lt;", f); nc = 0; break;
		case '>': fputs("&gt;", f); nc = 0; break;
		case '\n':
			nc++;
			if (nc <= 2)
				fputc('\n', f);
			break;
		}
		ptr++;
	}
	for (size_t i = nc; i <= 3; i++)
		fputc('\n', f);
	fclose(f);

	return 0;
}


/*
 * Root
 */

static char *copy_art_field(char **pptr)
{
	char *ptr = *pptr;
	while (*ptr != '"')
		ptr++;
	ptr++;
	char *p = ptr;
	while (*ptr != '"') {
		if (*ptr == '\\')
			ptr++;
		ptr++;
	}
	*ptr = 0;
	ptr++;
	*pptr = ptr;
	return string_copy(p);
}


art_root art_load(const char *path)
{
	size_t l = strlen(path);	
	art_root root = malloc(sizeof(*root));
	if (root == NULL)
		return NULL;

	int append_slash = path[l-1] != '/';
	root->dir = malloc(l + append_slash + 1);
	if (root->dir == NULL) {
		free(root);
		return NULL;
	}
	memcpy(root->dir, path, l);
	if (append_slash) {
		root->dir[l+0] = '/';
		root->dir[l+1] = 0;
	} else {
		root->dir[l] = 0;
	}

	char buf[256];
	memcpy(buf, path, l);
	memcpy(buf + l, ".list", sizeof(".list"));

	list arts = list_create(sizeof(article));
	FILE *f = fopen(buf, "r");
	article prev = NULL;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (buf[0] == '\n')
			continue;
		article a = malloc(sizeof(*a));
		char *ptr = buf;
		a->title  = copy_art_field(&ptr);
		a->author = copy_art_field(&ptr);
		char *date = copy_art_field(&ptr);
		a->date   = parse_date(date);
		free(date);
		a->file   = copy_art_field(&ptr);
		a->uri    = copy_art_field(&ptr);
		
		a->prev = prev;
		if (prev != NULL)
			prev->next = a;
		list_add(arts, &a);
		prev = a;
	}
	fclose(f);
	prev->next = NULL;
	root->articles = arts;

	return root;
}


void art_free(art_root root)
{
	free(root->dir);
	article a;
	size_t i = 0;
	while (list_iter(root->articles, &i, &a)) {
		free(a->title);
		free(a->author);
		free(a->file);
		free(a->uri);
	}
	list_free(root->articles);
	free(root);
}

/*
 * Article
 */
static list art_get_between_times(art_root root, struct date min, struct date max)
{
	list arts = list_create(sizeof(article));
	article a;
	size_t i = 0;
	while (list_iter(root->articles, &i, &a)) {
		if (min.num <= a->date.num && a->date.num < max.num)
			list_add(arts, &a);
	}
	return arts;
}


static uint64_t parse_uint(const char **pptr)
{
	const char *ptr = *pptr;
	uint64_t n = 0;
	while ('0' <= *ptr && *ptr <= '9') {
		n *= 10;
		n += *ptr - '0';
		ptr++;
	}
	*pptr = ptr;
	return n;
}


static uint8_t get_month_len(uint32_t y, uint8_t m)
{
	switch (m) {
		default:
			return -1;
		case 2:
			return (y % 4 == 0 && y % 100 != 0) ? 29 : 28;
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			return 31;
		case 4:
		case 6:
		case 9:
		case 11:
			return 30;
	}
}


static int uri_to_dates(struct date *min, struct date *max, const char *uri)
{
	min->num =  0;
	max->num = -1;
	const char *ptr = uri;
	if (*ptr == 0)
		return 0;

	min->year = max->year = parse_uint(&ptr);
	if (*ptr == 0)
		return 0;
	if (*ptr != '/')
		return -1;
	
	min->month = max->month = parse_uint(&ptr);
	if (*ptr == 0)
		return 0;
	if (*ptr != '/')
		return -1;
	if (min->month < 1 || min->month > 12)
		return -1;

	min->day = max->day = parse_uint(&ptr);
	if (*ptr == 0)
		return 0;
	if (*ptr != '/')
		return -1;
	if (min->day < 1 || min->day > get_month_len(min->year, min->month))
		return -1;

	ptr++;
	if (*ptr != 0)
		return -1;

	return 0;	
}


list art_get(art_root root, const char *uri) {
	if (*uri == 0 || ('0' <= *uri && *uri <= '9')) {
		struct date min, max;
		if (uri_to_dates(&min, &max, uri) < 0)
			return NULL;
		return art_get_between_times(root, min, max);
	} else {
		article *arts = malloc(sizeof(*arts));
		article  art  = arts[0] = calloc(1, sizeof(*art));

		article a;
		size_t i = 0;
		while (list_iter(root->articles, &i, &a)) {
			if (strcmp(a->uri, uri) == 0) {
				list l = list_create(sizeof(a));
				list_add(l, &a);
				return l;
			}
		}
		return NULL;
	}
}
