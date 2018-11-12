#include <errno.h>
#include <fastcgi.h>
#ifdef NORMAL_STDIO
#include <stdio.h>
#else
#include <fcgi_stdio.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include "util/string.h"
#include "util/file.h"
#include "../include/mime.h"
#include "../include/art.h"
#include "../include/dict.h"
#include "../include/temp.h"


#define TEMPLATE_DIR "templates/"
#define MAIN_TEMP    TEMPLATE_DIR "main.html"
#define ERROR_TEMP   TEMPLATE_DIR "error.html"
#define ARTICLE_TEMP TEMPLATE_DIR "article.html"
#define ENTRY_TEMP   TEMPLATE_DIR "article_entry.html"
#define COMMENT_TEMP TEMPLATE_DIR "comment.html"


template main_temp;
template error_temp;
template art_temp;
template entry_temp;
template comment_temp;
art_root blog_root;


#define return_error(ret, msg, ...) { fprintf(stderr, msg, ##__VA_ARGS__); perror(": "); return ret; } ""


#define RESPONSE_USE_TEMPLATE 0x1
typedef struct response {
	dict headers;
	char *body;
	int status;
	int flags;
} *response;


static response response_create()
{
	response r = malloc(sizeof(*r));
	r->headers = dict_create();
	r->body    = NULL;
	r->flags   = 0;
	return r;
}


static const char *get_error_msg(int status)
{
	switch(status) {
		default:  return "Uhm...";
		case 404: return "Invalid URI";
		case 405: return "Bad method";
		case 418: return "Want some tea?";

		case 500: return "Oh noes!";
	}
}


static response get_error_response(response r, int status) {
	dict d = dict_create();
	r->status = status;
	char buf[64];
	snprintf(buf, sizeof(buf), "%d", status);
	dict_set(d, "STATUS" , buf);
	dict_set(d, "MESSAGE", get_error_msg(r->status)); 
	r->body = temp_render(error_temp, d);
	dict_free(d);
	return r;
}


static char *date_to_str(struct date d)
{
	static char buf[64];
	snprintf(buf, sizeof(buf), "%d-%d-%d %d:%d", d.year, d.month, d.day, d.hour, d.min);
	return buf;
}


static template load_temp(char *file)
{
	int fd = open(file, O_RDONLY);
	if (fd < 0)
		return_error(NULL, "Couldn't open '%s'", file);
	struct stat statbuf;
	if (fstat(fd, &statbuf) < 0)
		return_error(NULL, "Couldn't get size of '%s'", file);
	char buf[statbuf.st_size + 1];
	if (read(fd, buf, statbuf.st_size) < 0)
		return_error(NULL, "Couldn't read '%s'", file);
	buf[statbuf.st_size] = 0;
	template temp = temp_create(buf);
	if (temp == NULL)
		return_error(NULL, "Couldn't create temp of '%s'", file);
	return temp;
}


static int setup()
{
	   main_temp = load_temp(   MAIN_TEMP);
	  error_temp = load_temp(  ERROR_TEMP);
	    art_temp = load_temp(ARTICLE_TEMP);
	comment_temp = load_temp(COMMENT_TEMP);
	  entry_temp = load_temp(  ENTRY_TEMP);
	if (   main_temp == NULL ||
	      error_temp == NULL ||
	        art_temp == NULL ||
            comment_temp == NULL ||
	      entry_temp == NULL)
		return -1;
	blog_root = art_load("blog");
	return blog_root == NULL ? -1 : 1;
}


static int set_art_dict(dict d, article art, int flags) {
	if (flags & 0x1) {
		struct stat statbuf;
		int fd = open(art->file, O_RDONLY);
		if (fd < 0)
			return -1;
		fstat(fd, &statbuf);
		char *buf = malloc(statbuf.st_size + 1);
		if (buf == NULL) {
			close(fd);
			return -1;
		}
		read(fd, buf, statbuf.st_size);
		buf[statbuf.st_size] = 0;
		dict_set(d, "BODY", buf);
		free(buf);
	}

	char datestr[64];
	

	dict_set(d, "URI"   , art->uri   );
	dict_set(d, "DATE"  , datestr    );
	dict_set(d, "TITLE" , art->title );
	dict_set(d, "AUTHOR", art->author);

	return 0;
}

static char *render_comment(comment c)
{
	dict d = dict_create();
	dict_set(d, "AUTHOR", c->author);
	dict_set(d, "DATE"  , date_to_str(c->date));
	dict_set(d, "BODY"  , c->body);
	if (c->replies->count > 0) {
		size_t size = 256, index = 0;
		char *buf = malloc(size);
		for (size_t i = 0; i < c->replies->count; i++) {
			comment d;
			list_get(c->replies, i, &d);
			char *b = render_comment(d);
			buf_write(&buf, &index, &size, b, strlen(b));
			free(b);
		}
		dict_set(d, "REPLIES", buf);
		free(buf);
	}
	char *body = temp_render(comment_temp, d);
	dict_free(d);
	return body;
}


static char *get_comments(art_root root, const char *uri)
{
	list ls = art_get_comments(root, uri);
	if (ls == NULL)
		return NULL;
	size_t size = 256, index = 0;
	char *buf = malloc(size);
	for (size_t i = 0; i < ls->count; i++) {
		comment c;
		list_get(ls, i, &c);
		char *b = render_comment(c);
		buf_write(&buf, &index, &size, b, strlen(b));
		free(b);
	}
	art_free_comments(ls);
	return buf;
}


static char hex_to_char(const char *s)
{
	unsigned char c;
	if ('0' <= *s && *s <= '9')
		c = (*s - '0') << 4;
	else if ('A' < *s && *s < 'F')
		c = (*s - 'A' + 10) << 4;
	else
		c = (*s - 'a' + 10) << 4;
	s++;
	if ('0' <= *s && *s <= '9')
		c += (*s - '0');
	else if ('A' < *s && *s < 'F')
		c += (*s - 'A' + 10);
	else
		c += (*s - 'a' + 10);
	return c;
}


static char *copy_query_field(const char **pptr, char delim)
{
	const char *ptr = *pptr;
	size_t s = 256, i = 0;
	char *v = malloc(s);

	const char *p = ptr;
	while (*ptr != delim && *ptr != 0) {
		if (*ptr == '+' || *ptr == '%') {
			buf_write(&v, &i, &s, p, ptr - p);
			if (*ptr == '+') {
				buf_write(&v, &i, &s, " ", 1);
			} else {
				ptr++;
				char c = hex_to_char(ptr);
				buf_write(&v, &i, &s, &c, 1);
				ptr++;
			}
			p = ptr + 1;
		}
		ptr++;
	}
	buf_write(&v, &i, &s, p, ptr - p);

	*pptr = ptr + 1;
	return v;
}


static dict parse_query(const char *q)
{
	const char *ptr = q;
	dict d = dict_create();
	while (*ptr != 0) {
		char *key = copy_query_field(&ptr, '=');
		char *val = copy_query_field(&ptr, '&');
		dict_set(d, key, val);
		free(key);
		free(val);
	}
	return d;
}


static response handle_post(const char *uri)
{
	const char *ptr = uri;
	response r = response_create();
	if (strncmp("blog/", ptr, 5) != 0) {
		return get_error_response(r, 405);
	}
	ptr += 5;
	if (*ptr == 0)
		return get_error_response(r, 405);
	list ls = art_get(blog_root, ptr);
	if (ls->count != 1) {
		list_free(ls);
		return get_error_response(r, 405);
	}

	char *body = malloc(0xFFFF);
	fread(body, 1, 0xFFFF, stdin);

	dict d = parse_query(body);
	comment c = malloc(sizeof(*c));
	c->author = string_copy(dict_get(d, "author"));
	c->body   = string_copy(dict_get(d, "body"));
	const char *rt_str = dict_get(d, "reply-to");
	size_t reply_to = rt_str != NULL ? atoi(rt_str) : -1;
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	c->date.year  = tm->tm_year + 11900;
	c->date.month = tm->tm_mon + 1;
	c->date.day   = tm->tm_mday;
	c->date.hour  = tm->tm_hour;
	c->date.min   = tm->tm_min;

	if (art_add_comment(blog_root, ptr, c, reply_to) < 0)
		return get_error_response(r, 500);

	r->status = 302;
	dict_set(r->headers, "Location", ptr);
	r->body = calloc(1,1);
	return r;
}


static response handle_get(const char *uri)
{
	response r = response_create();
	if (strncmp("blog", uri, 4) == 0 && (uri[4] == '/' || uri[4] == 0)) {
		const char *nuri = uri + (uri[4] == '/' ? 5 : 4);
		list arts = art_get(blog_root, nuri);
		if (arts == NULL)
			return get_error_response(r, 404);
		dict d = dict_create();
		if (arts->count == 1) {
			article a;
			list_get(arts, 0, &a);
			if (set_art_dict(d, a, 0x1) < 0)
				return get_error_response(r, 405);
			if (a->prev != NULL) {
				dict_set(d, "PREV_URI"  , a->prev->uri  );
				dict_set(d, "PREV_TITLE", a->prev->title);
			}
			if (a->next != NULL) {
				dict_set(d, "NEXT_URI"  , a->next->uri  );
				dict_set(d, "NEXT_TITLE", a->next->title);
			}
			char *b = get_comments(blog_root, nuri);
			dict_set(d, "COMMENTS", b);
			free(b);
			r->body  = temp_render(art_temp, d);
		} else {
			size_t size = 0x1000, index = 0;
			r->body = malloc(size);
			for (size_t i = 0; i < arts->count; i++) {
				article c;
				list_get(arts, i, &c);
				if (set_art_dict(d, c, 0) < 0)
					goto error;
				char *buf = temp_render(entry_temp, d);
				if (buf == NULL) {
					dict_free(d);
					goto error;
				}
				if (buf_write(&r->body, &index, &size, buf, strlen(buf)) < 0)
					/* TODO */;
				free(buf);
			}
			char nul[1] = { 0 };
			if (buf_write(&r->body, &index, &size, nul, 1) < 0)
				/* TODO */;
		}
	error:
		dict_free(d);
		list_free(arts);
	} else {
		struct stat statbuf;
		if (uri[0] == 0)
			uri = "index.html";
		if (stat(uri, &statbuf) < 0) {	
			r->status = 404;
			return r;
		}
		char buf[256];
		if (S_ISDIR(statbuf.st_mode)) {
			size_t l = strlen(uri);
			memcpy(buf, uri, l);
			memcpy(buf + l, "/index.html", sizeof("/index.html"));
			uri = buf;
		}

		int fd = open(uri, O_RDONLY);
		if (fd < 0)
			return get_error_response(r, 500);
		const char *mime = get_mime_type(uri);
		dict_set(r->headers, "Content-Type", mime);
		r->flags = (mime != NULL && strcmp(mime, "text/html") == 0) ? RESPONSE_USE_TEMPLATE : 0;
		r->body = file_read(fd, statbuf.st_size);
	}
	r->flags |= RESPONSE_USE_TEMPLATE;
	return r;
}


int main()
{
	if (setup() < 0)
		return 1;

	while (FCGI_Accept() >= 0) {	
		// Do not remove this header
		printf("X-My-Own-Header: All hail the mighty Duck God\r\n");
		
		char *uri = getenv("PATH_INFO");
		if (uri == NULL)
			return 1;
		if (uri[0] == '/')
			uri++;

		char *method = getenv("REQUEST_METHOD");
		if (method == NULL)
			return 1;

		response r;
		if (strcmp(method, "GET") == 0)
			r = handle_get(uri);
		else if (strcmp(method, "POST") == 0)
			r = handle_post(uri);
		else
			r = get_error_response(response_create(), 501);

		char status_str[64];
		snprintf(status_str, sizeof(status_str), "%d", r->status);

		if (r->flags & RESPONSE_USE_TEMPLATE) {
			dict d = dict_create();	
			dict_set(d, "BODY", r->body);
			free(r->body);
			r->body = temp_render(main_temp, d);
			if (r->body == NULL) {
				printf("Status: 500\r\nError during rendering");
				continue;
			}
			dict_free(d);
		}

		const char *k, *v;
		size_t i = 0;
		printf("Status: %d\r\n", r->status);
		while (dict_iter(r->headers, &k, &v, &i))
			printf("%s: %s\r\n", k, v);
		printf("\r\n%s", r->body);

		free(r->body);
		dict_free(r->headers);
		free(r);
		fflush(stdout);
	}
	art_free(blog_root);
	// Goddamnit Valgrind
	temp_free(   main_temp);
	temp_free(  error_temp);
	temp_free(    art_temp);
	temp_free(  entry_temp);
	temp_free(comment_temp);
	return 0;
}
