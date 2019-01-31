#include "../include/article.h"
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
#include "temp-alloc.h"
#include "cinja.h"
#include "temp/cinja.h"


static string comment_path_component;

__attribute__((constructor))
void init()
{
	comment_path_component = string_create("comments/");
}


/*
 * Helpers
 */
static struct date parse_date(string str)
{
	struct date date = { .num = 0 };
	uint32_t Y, M, d, h, m;
	int n = sscanf(str->buf, "%d-%d-%d %d:%d", &Y, &M, &d, &h, &m);
	switch (n) {
	case 5: date.min   = m;
	case 4: date.hour  = h;
	case 3: date.day   = d;
	case 2: date.month = M;
	case 1: date.year  = Y;
	}
	return date;
}


/*
 * Comments
 */

static comment parse_comment(const string str)
{
	comment c = temp_alloc(sizeof(*c));
	size_t i = 0, start = i;
	while (str->buf[i] != '\n')
		i++;
	c->author = temp_string_copy(str, start, i);
	i++;

	start = i;
	while (str->buf[i] != '\n')
		i++;
	string date = temp_string_copy(str, start, i);
	c->date = parse_date(date);
	i++;

	start = i;
	while (str->buf[i] != '\n')
		i++;
	sscanf(str->buf + start, "%d", &c->reply_to);
	i++;

	c->body = temp_string_copy(str, i, str->len);

	c->replies = cinja_temp_list_create();

	return c;
}


cinja_list art_get_comments(art_root root, const string name)
{
	const char **entries  = NULL;
	comment     *comments = NULL;
	cinja_list  ls        = NULL;

	string file_components[3] = { root->dir, comment_path_component, name };
	string file = temp_string_concat(file_components, 3);
	FILE *f = fopen(file->buf, "r");
	string str;
	if (f == NULL) {
		str = temp_string_create("");
	} else {
		fseek(f, 0, SEEK_END);
		size_t s = ftell(f);
		fseek(f, 0, SEEK_SET);
		str = temp_alloc(sizeof(str->len) + s + 1);
		fread(str->buf, 1, s, f);
		str->buf[s] = 0;
		str->len = s;
		fclose(f);
	}

	cinja_list cs = cinja_temp_list_create();
	size_t i = 0;
	for (int id = 0; i < str->len; id++) {
		while (str->buf[i] == '\n')
			i++;
		size_t start = i;
		while (1) {
			if (memcmp(&str->buf[i], "\n\n\n", 3) == 0)
				break;
			if (i >= str->len)
				goto done;
			i++;
		}
		string cstr = string_copy(str, start, i);
		comment c = parse_comment(cstr);
		free(cstr);
		c->id = id;
		cinja_list_add(cs, c);
		i++;
	}
done:
	ls = cinja_temp_list_create(sizeof(comment));
	for (size_t i = 0; i < cs->count; i++) {
		comment c = cinja_list_get(cs, i).item;
		cinja_list l;
		if (c->reply_to == -1) {
			l = ls;
		} else {
			comment d = cinja_list_get(cs, c->reply_to).item;
			l = d->replies;
		}
		if (cinja_list_add(l, c) < 0)
			goto error;
	}

	goto success;
error:
	if (ls != NULL)
		cinja_list_free(ls);
	ls = NULL;
success:
	free(entries);
	free(comments);
	return ls;
}


int art_add_comment(art_root root, const string uri, comment c, size_t reply_to)
{
	// Search for the article
	for (size_t i = 0; i < root->articles->count; i++) {
		article a = cinja_list_get(root->articles, i).item;
		if (string_eq(a->uri, uri))
			goto found;
	}
	return -1;

found:;
	// Open the comment file
	string file_components[3] = { root->dir, comment_path_component, uri };
	string file = temp_string_concat(file_components, 3);
	FILE *f = fopen(file->buf, "a");
	if (f == NULL) {
		f = fopen(file->buf, "w");
		if (f == NULL)
			return -1;
	}
	struct date d = c->date;

	// Write the author's name (without newlines)
	for (size_t i = 0; i < c->author->len; i++) {
		char x = c->author->buf[i];
		switch (x) {
		default  : fputc(x     , f); break;
		case  '<': fputs("&lt;", f); break;
		case  '>': fputs("&gt;", f); break;
		case '\n': break;
		}
	}
	fputc('\n', f);

	// Write the date and reply ID
	fprintf(f,
	        "%u-%u-%u %u:%u\n"
	        "%ld\n",
	        d.year, d.month, d.day, d.hour, d.min,
	        reply_to);

	// Write the body with trimmed newlines
	int nc = 0;
	for (size_t i = 0; i < c->body->len; i++) {
		char x = c->body->buf[i];
		switch (x) {
		default : fputc(x     , f); nc = 0; break;
		case '<': fputs("&lt;", f); nc = 0; break;
		case '>': fputs("&gt;", f); nc = 0; break;
		case '\n':
			nc++;
			if (nc <= 2)
				fputc('\n', f);
			break;
		}
	}

	// Write the delimiter
	for (size_t i = nc; i <= 3; i++)
		fputc('\n', f);

	// Done
	fclose(f);
	return 0;
}


/*
 * Root
 */

static string copy_art_field(char **pptr)
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
	return string_create(p);
}


art_root art_load(const string path)
{
	art_root root = malloc(sizeof(*root));
	if (root == NULL)
		return NULL;

	root->dir = malloc(sizeof(root->dir->len) + path->len + 2);
	if (root->dir == NULL) {
		free(root);
		return NULL;
	}
	root->dir->len = path->len + 1;
	memcpy(root->dir->buf, path->buf, path->len);
	root->dir->buf[path->len+0] = '/';
	root->dir->buf[path->len+1] = 0;

	char buf[256];
	memcpy(buf, path->buf, path->len);
	memcpy(buf + path->len, ".list", sizeof(".list"));

	cinja_list arts = cinja_list_create(sizeof(article));
	FILE *f = fopen(buf, "r");
	article prev = NULL;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (buf[0] == '\n')
			continue;
		article a   = malloc(sizeof(*a));
		char *ptr   = buf;
		a->title    = copy_art_field(&ptr);
		string date = copy_art_field(&ptr);
		a->date     = parse_date(date);
		free(date);
		a->file     = copy_art_field(&ptr);
		a->uri      = copy_art_field(&ptr);
		a->prev = prev;
		if (prev != NULL)
			prev->next = a;
		cinja_list_add(arts, a);
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
	for (size_t i = 0; i < root->articles->count; i++) {
		article a = cinja_list_get(root->articles, i).item;
		free(a->title);
		free(a->file);
		free(a->uri);
		free(a);
	}
	cinja_list_free(root->articles);
	free(root);
}

/*
 * Article
 */
static cinja_list art_get_between_times(art_root root, struct date min, struct date max)
{
	cinja_list arts = cinja_temp_list_create(sizeof(article));
	for (size_t i = 0; i < root->articles->count; i++) {
		article a = cinja_list_get(root->articles, i).item;
		if (min.num <= a->date.num && a->date.num < max.num)
			cinja_list_add(arts, a);
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


cinja_list art_get(art_root root, const string uri) {
	if (uri->buf[0] == 0 || ('0' <= uri->buf[0] && uri->buf[0] <= '9')) {
		struct date min, max;
		if (uri_to_dates(&min, &max, uri->buf) < 0)
			return NULL;
		return art_get_between_times(root, min, max);
	} else {
		article *arts = temp_alloc(sizeof(*arts));
		article  art  = arts[0] = temp_alloc(sizeof(*art));

		for (size_t i = 0; i < root->articles->count; i++) {
			article a = cinja_list_get(root->articles, i).item;
			if (string_eq(a->uri, uri)) {
				cinja_list l = cinja_temp_list_create(sizeof(a));
				cinja_list_add(l, a);
				return l;
			}
		}
		return NULL;
	}
}
