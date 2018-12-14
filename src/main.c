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
#include "../include/mime.h"
#include "../include/art.h"
#include "../include/dict.h"
#include "cinja.h"


#define TEMPLATE_DIR "templates/"
#define MAIN_TEMP    TEMPLATE_DIR "main.html"
#define ERROR_TEMP   TEMPLATE_DIR "error.html"
#define ARTICLE_TEMP TEMPLATE_DIR "article.html"
#define ENTRY_TEMP   TEMPLATE_DIR "article_list.html"
#define COMMENT_TEMP TEMPLATE_DIR "comment.html"


cinja_template main_temp;
cinja_template error_temp;
cinja_template art_temp;
cinja_template entry_temp;
cinja_template comment_temp;
art_root blog_root;


#define return_error(ret, msg, ...) { fprintf(stderr, msg, ##__VA_ARGS__); perror(": "); return ret; } ""


#define RESPONSE_USE_TEMPLATE 0x1
typedef struct response {
	cinja_dict headers;
	string body;
	int status;
	int flags;
} *response;


static response response_create()
{
	response r = malloc(sizeof(*r));
	r->headers = cinja_dict_create();
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
	cinja_dict d = cinja_dict_create();
	r->status = status;
	r->flags  = RESPONSE_USE_TEMPLATE;
	char buf[64];
	snprintf(buf, sizeof(buf), "%d", status);
	cinja_dict_set(d, string_create("STATUS" ), string_create(buf));
	cinja_dict_set(d, string_create("MESSAGE"), string_create(get_error_msg(r->status))); 
	r->body = cinja_render(error_temp, d);
	cinja_dict_free(d);
	return r;
}


static string date_to_str(struct date d)
{
	static char buf[64];
	snprintf(buf, sizeof(buf), "%d-%d-%d %d:%d", d.year, d.month, d.day, d.hour, d.min);
	return string_create(buf);
}


static cinja_template load_temp(char *file)
{
	cinja_template temp = cinja_create_from_file(file);
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
	string s = string_create("blog");
	blog_root = art_load(s);
	free(s);
	return blog_root == NULL ? -1 : 1;
}


static int set_art_dict(cinja_dict d, article art, int flags) {
	if (flags & 0x1) {
		struct stat statbuf;
		int fd = open(art->file->buf, O_RDONLY);
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
		cinja_dict_set(d, string_create("BODY"), string_create(buf));
		free(buf);
	}

	struct date t = art->date;
	char buf[64];
	snprintf(buf, sizeof(buf), "%d-%d-%d %d:%d",
			t.year, t.month, t.day, t.hour, t.min);

	cinja_dict_set(d, string_create("URI"   ), art->uri);
	cinja_dict_set(d, string_create("DATE"  ), string_create(buf));
	cinja_dict_set(d, string_create("TITLE" ), art->title);
	cinja_dict_set(d, string_create("AUTHOR"), art->author);

	return 0;
}

static cinja_dict _comment_to_dict(comment c)
{
	char idbuf[64];
	snprintf(idbuf, sizeof(idbuf), "%d", c->id);
	cinja_dict d = cinja_dict_create();
	cinja_dict_set(d, string_create("AUTHOR"), c->author);
	cinja_dict_set(d, string_create("DATE"  ), date_to_str(c->date));
	cinja_dict_set(d, string_create("BODY"  ), c->body);
	cinja_dict_set(d, string_create("ID"    ), string_create(idbuf));
	if (c->replies->count > 0) {
		cinja_list replies = cinja_list_create();
		for (size_t i = 0; i < c->replies->count; i++) {
			comment d = cinja_list_get(c->replies, i).item;
			cinja_list_add(replies, _comment_to_dict(d));
		}
		cinja_dict_set(d, string_create("REPLIES"), replies);
		cinja_dict_set(d, string_create("comment"), comment_temp);
	}
	return d;
}


static cinja_list get_comments(art_root root, const string uri)
{
	cinja_list ls = art_get_comments(root, uri);
	if (ls == NULL)
		return NULL;
	cinja_list comments = cinja_list_create();
	for (size_t i = 0; i < ls->count; i++) {
		comment c = cinja_list_get(ls, ls->count - i - 1).item;
		cinja_list_add(comments, _comment_to_dict(c));
	}
	art_free_comments(ls);
	return comments;
}


static char hex_to_char(const char *s)
{
	unsigned char c;
	if ('0' <= *s && *s <= '9')
		c = (*s - '0') << 4;
	else if ('A' <= *s && *s <= 'F')
		c = (*s - 'A' + 10) << 4;
	else
		c = (*s - 'a' + 10) << 4;
	s++;
	if ('0' <= *s && *s <= '9')
		c += (*s - '0');
	else if ('A' <= *s && *s <= 'F')
		c += (*s - 'A' + 10);
	else
		c += (*s - 'a' + 10);
	return c;
}


static string copy_query_field(const char **pptr, char delim)
{
	const char *ptr = *pptr;
	size_t s = 4000, i = 0;
	string v = malloc(sizeof(v->len) + s + 1);
	if (v == NULL)
		return NULL;

	const char *p = ptr;
	for (v->len = 0; *ptr != delim && *ptr != 0; v->len++) {
		if (s <= i) {
			string tmp = realloc(v, s * 3 / 2);
			if (tmp == NULL) {
				free(v);
				return NULL;
			}
			v = tmp;
			s = s * 3 / 2;
		}
		if (*ptr == '+' || *ptr == '%') {
			if (*ptr == '+') {
				v->buf[v->len] = ' ';
			} else {
				ptr++;
				v->buf[v->len] = hex_to_char(ptr);
				ptr++;
			}
			p = ptr + 1;
		} else {
			v->buf[v->len] = *ptr;
		}
		ptr++;
	}
	v->buf[v->len] = 0;

	*pptr = ptr + 1;
	return v;
}


static cinja_dict parse_query(const char *q)
{
	const char *ptr = q;
	cinja_dict d = cinja_dict_create();
	while (*ptr != 0) {
		string key = copy_query_field(&ptr, '=');
		string val = copy_query_field(&ptr, '&');
		cinja_dict_set(d, key, val);
	}
	return d;
}


static response handle_post(const string uri)
{
	response r = response_create();
	if (strncmp("blog/", uri->buf, 5) != 0) {
		return get_error_response(r, 405);
	}
	if (uri->buf[5] == 0)
		return get_error_response(r, 405);
	cinja_list ls = art_get(blog_root, string_create(uri->buf + 5));
	if (ls->count != 1) {
		cinja_list_free(ls);
		return get_error_response(r, 405);
	}

	char *body = malloc(0xFFFF);
	size_t end = fread(body, 1, 0xFFFF, stdin);
	body[end] = 0;

	cinja_dict d = parse_query(body);
	comment c = malloc(sizeof(*c));
	c->author = string_copy(cinja_dict_get(d, string_create("author")).value);
	c->body   = string_copy(cinja_dict_get(d, string_create("body")).value);
	const char *rt_str = cinja_dict_get(d, string_create("reply-to")).value;
	size_t reply_to = rt_str != NULL ? atoi(rt_str) : -1;
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	c->date.year  = tm->tm_year + 11900;
	c->date.month = tm->tm_mon + 1;
	c->date.day   = tm->tm_mday;
	c->date.hour  = tm->tm_hour;
	c->date.min   = tm->tm_min;

	if (art_add_comment(blog_root, string_create(uri->buf + 5), c, reply_to) < 0)
		return get_error_response(r, 500);

	r->status = 302;
	cinja_dict_set(r->headers, string_create("Location"), string_create(uri->buf + 5));
	r->body = calloc(1,1);
	return r;
}


static response handle_get(const string uri)
{
	response r = response_create();
	if (strncmp("blog", uri->buf, 4) == 0 && (uri->buf[4] == '/' || uri->buf[4] == 0)) {
		r->flags = RESPONSE_USE_TEMPLATE;
		const string nuri = string_create(uri->buf + (uri->buf[4] == '/' ? 5 : 4));
		cinja_list arts = art_get(blog_root, nuri);
		if (arts == NULL)
			return get_error_response(r, 404);
		if (arts->count == 1) {
			cinja_dict d = cinja_dict_create();
			article a = cinja_list_get(arts, 0).item;
			if (set_art_dict(d, a, 0x1) < 0)
				return get_error_response(r, 500);
			if (a->prev != NULL) {
				cinja_dict_set(d, string_create("PREV_URI"  ), string_copy(a->prev->uri  ));
				cinja_dict_set(d, string_create("PREV_TITLE"), string_copy(a->prev->title));
			}
			if (a->next != NULL) {
				cinja_dict_set(d, string_create("NEXT_URI"  ), string_copy(a->next->uri  ));
				cinja_dict_set(d, string_create("NEXT_TITLE"), string_copy(a->next->title));
			}
			cinja_list comments = get_comments(blog_root, nuri);
			cinja_dict_set(d, string_create("COMMENTS"), comments);
			cinja_dict_set(d, string_create("comment" ), comment_temp);
			r->body  = cinja_render(art_temp, d);
			cinja_dict_free(d);
		} else {
			cinja_list dicts    = cinja_list_create();
			for (size_t i = 0; i < arts->count; i++) {
				cinja_dict d = cinja_dict_create();
				article    c = cinja_list_get(arts, i).item;
				if (set_art_dict(d, c, 0) < 0)
					goto error;
				cinja_list_add(dicts, d);
			}
			cinja_dict dict = cinja_dict_create();
			cinja_dict_set(dict, string_create("ARTICLES"), dicts);
			r->body = cinja_render(entry_temp, dict);
			cinja_dict_free(dict);
			char buf1[64], buf2[64];
			string bodystr = (void *)buf1, datestr = (void *)buf2;
			string_create("BODY", 4, bodystr);
			string_create("DATE", 4, datestr);
			for (size_t i = 0; i < dicts->count; i++) {
				cinja_dict d = dicts->items[i].item;
				free(cinja_dict_get(d, bodystr).value);
				free(cinja_dict_get(d, datestr).value);
				cinja_dict_free(d);
			}
			cinja_list_free(dicts);
		}
	error:
		free(nuri);
		cinja_list_free(arts);
	} else {
		string path;
		if (uri->len == 0) {
			path = string_create("index.html");
		} else {
			struct stat statbuf;
			string components[2] = { uri };

			if (stat(uri->buf, &statbuf) < 0) {
				components[1] = string_create(".html");
			} else {
				if (S_ISDIR(statbuf.st_mode))
					components[1] = string_create("/index.html");
				else
					components[1] = string_create("");
			}
			path = string_concat(components, 2);
			free(components[1]);
		}

		const string mime = get_mime_type(path);
		cinja_dict_set(r->headers, string_create("Content-Type"), mime);
		r->flags = (mime != NULL && strcmp(mime->buf, "text/html") == 0) ? RESPONSE_USE_TEMPLATE : 0;

		FILE *f = fopen(path->buf, "r");
		if (f == NULL)
			return get_error_response(r, 500);
		fseek(f, 0, SEEK_END);
		size_t s = ftell(f);
		fseek(f, 0, SEEK_SET);
		r->body = malloc(sizeof(r->body->len) + s + 1);
		fread(r->body->buf, s, 1, f);
		r->body->buf[s] = 0;
		fclose(f);
		r->body->len = s;
		free(path);
	}
	r->status = 200;
	return r;
}


int main()
{
	if (setup() < 0)
		return 1;

	while (FCGI_Accept() >= 0) {	
		// Do not remove this header
		printf("X-My-Own-Header: All hail the mighty Duck God\r\n");
		
		char *uri_a = getenv("PATH_INFO");
		if (uri_a == NULL)
			return 1;
		if (uri_a[0] == '/')
			uri_a++;
		size_t uri_a_l = strlen(uri_a);
		if (uri_a_l > 0 && uri_a[uri_a_l - 1] == '/')
			uri_a_l--;
		string uri = string_create(uri_a, uri_a_l);

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
			cinja_dict d = cinja_dict_create();
			string     b = r->body;
			cinja_dict_set(d, string_create("BODY"), r->body);
			r->body = cinja_render(main_temp, d);
			if (r->body == NULL) {
				printf("Status: 500\r\nError during rendering");
				continue;
			}
			free(b);
			cinja_dict_free(d);
		}

		void *state = NULL;
		printf("Status: %d\r\n", r->status);
		for (cinja_dict_entry_t e = cinja_dict_iter(r->headers, &state); e.value != NULL; e = cinja_dict_iter(r->headers, &state))
			printf("%s: %s\r\n", e.key->buf, ((string)e.value)->buf);
		printf("\r\n%s", r->body->buf);

		free(r->body);
		cinja_dict_free(r->headers);
		free(r);
		free(uri);
		fflush(stdout);
	}
	art_free(blog_root);
	// Goddamnit Valgrind
	cinja_free(   main_temp);
	cinja_free(  error_temp);
	cinja_free(    art_temp);
	cinja_free(  entry_temp);
	cinja_free(comment_temp);
	return 0;
}
