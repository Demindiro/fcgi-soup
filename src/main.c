#include <errno.h>
#include <fastcgi.h>
#include <fcgi_stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include "../include/mime.h"
#include "../include/article.h"
#include "../include/dict.h"
#include "temp-alloc.h"
#include "temp/dict.h"
#include "cinja.h"


// Constants
#define TEMPLATE_DIR "templates/"
#define MAIN_TEMP    TEMPLATE_DIR "main.html"
#define ERROR_TEMP   TEMPLATE_DIR "error.html"
#define ARTICLE_TEMP TEMPLATE_DIR "article.html"
#define ENTRY_TEMP   TEMPLATE_DIR "article_list.html"
#define COMMENT_TEMP TEMPLATE_DIR "comment.html"
#define RESPONSE_USE_TEMPLATE 0x1


// Global variables
cinja_template   main_temp;
cinja_template   error_temp;
cinja_template     art_temp;
cinja_template   entry_temp;
cinja_template comment_temp;
art_root          blog_root;
char redirect_tls = 0;
string author_name;


// Macros
#define RETURN_ERROR(ret, msg, ...) { fprintf(stderr, msg, ##__VA_ARGS__); perror(": "); return ret; } ""


// Structs
typedef struct response {
	cinja_dict headers;
	string body;
	int status;
	int flags;
} *response;


static response response_create()
{
	response r = temp_alloc(sizeof(*r));
	r->headers = cinja_temp_dict_create();
	r->body    = NULL;
	r->flags   = 0;
	return r;
}


static const char *get_error_msg(int status)
{
	switch(status) {
		default:  return "Uhm...";

		// 4xx
		case 400: return "You made a mistake somewhere. Or maybe the developer. Dunno";
		case 404: return "Invalid URI";
		case 405: return "Bad method";
		case 418: return "Want some tea?";

		// 5xx
		case 500: return "Internal error";
	}
}


static response get_error_response(response r, int status) {
	cinja_dict d = cinja_temp_dict_create();
	r->status = status;
	r->flags  = RESPONSE_USE_TEMPLATE;
	char buf[64];
	snprintf(buf, sizeof(buf), "%d", status);
	cinja_dict_set(d, temp_string_create("STATUS" ), temp_string_create(buf));
	cinja_dict_set(d, temp_string_create("MESSAGE"),
	               temp_string_create(get_error_msg(r->status)));
	r->body = cinja_temp_render(error_temp, d);
	return r;
}


static string date_to_str(struct date d)
{
	static char buf[64];
	snprintf(buf, sizeof(buf), "%d-%d-%d %d:%d", d.year, d.month, d.day, d.hour, d.min);
	return temp_string_create(buf);
}


static cinja_template load_temp(char *file)
{
	cinja_template temp = cinja_create_from_file(file);
	if (!temp)
		RETURN_ERROR(NULL, "Couldn't create temp of '%s'", file);
	return temp;
}


static int setup()
{
	// Load the configuration file
	FILE *f = fopen("soup.conf", "r");
	if (!f)
		RETURN_ERROR(-1, "Failed to open soup.conf");
	char buf[1 << 16];
	for (size_t line = 1; fgets(buf, sizeof(buf), f); line++) {
		char *ptr = buf, *orgptr = ptr;
		if (*ptr == '\n')
			continue;
		while (*ptr != ' ' && *ptr != '\t')
		{
			if (*ptr == '\n' || *ptr == 0)
				RETURN_ERROR(-1, "Syntax error in soup.conf:%lu", line);
			ptr++;
		}
		size_t strl = ptr - orgptr;
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;
		switch (strl)
		{
		case 3:
			if (strncmp(orgptr, "tls", 3) == 0) {
				redirect_tls = *ptr - '0';
				break;
			}
		case 6:
			if (strncmp(orgptr, "author", 6) == 0) {
				orgptr = ptr;
				while (*ptr != '\n' && *ptr != 0)
					ptr++;
				author_name = string_create(orgptr, ptr - orgptr);
				break;
			}
		default:
			RETURN_ERROR(-1, "Unknown option: %*s", (int)(ptr - orgptr), orgptr);
		}
	}

	// Load the templates
	   main_temp = load_temp(   MAIN_TEMP);
	  error_temp = load_temp(  ERROR_TEMP);
	    art_temp = load_temp(ARTICLE_TEMP);
	comment_temp = load_temp(COMMENT_TEMP);
	  entry_temp = load_temp(  ENTRY_TEMP);
	if (!main_temp || !error_temp || !art_temp || !comment_temp || !entry_temp)
		return -1;
	blog_root = art_load(temp_string_create("blog"));
	return blog_root ? 0 : -1;
}


static int set_article_dict(cinja_dict d, article art, int load_body) {
	if (load_body) {
		struct stat statbuf;
		int fd = open(art->file->buf, O_RDONLY);
		if (fd < 0)
			return -1;
		fstat(fd, &statbuf);
		char *buf = malloc(statbuf.st_size + 1);
		if (!buf) {
			close(fd);
			return -1;
		}
		read(fd, buf, statbuf.st_size);
		buf[statbuf.st_size] = 0;
		cinja_dict_set(d, temp_string_create("BODY"), temp_string_create(buf));
		free(buf);
	}

	struct date t = art->date;
	char buf[64];
	snprintf(buf, sizeof(buf), "%d-%d-%d %d:%d",
	         t.year, t.month, t.day, t.hour, t.min);

	cinja_dict_set(d, temp_string_create("URI"   ), art->uri);
	cinja_dict_set(d, temp_string_create("DATE"  ), temp_string_create(buf));
	cinja_dict_set(d, temp_string_create("TITLE" ), art->title);
	cinja_dict_set(d, temp_string_create("AUTHOR"), author_name);

	return 0;
}


/**
Comments
*/

static cinja_dict _comment_to_dict(comment c)
{
	char idbuf[64];
	snprintf(idbuf, sizeof(idbuf), "%d", c->id);
	cinja_dict d = cinja_temp_dict_create();
	cinja_temp_dict_set(d, temp_string_create("AUTHOR"), c->author);
	cinja_temp_dict_set(d, temp_string_create("DATE"  ), date_to_str(c->date));
	cinja_temp_dict_set(d, temp_string_create("BODY"  ), c->body);
	cinja_temp_dict_set(d, temp_string_create("ID"    ), temp_string_create(idbuf));
	if (c->replies->count > 0) {
		cinja_list replies = cinja_temp_list_create();
		for (size_t i = 0; i < c->replies->count; i++) {
			comment d = cinja_list_get(c->replies, i).item;
			cinja_list_add(replies, _comment_to_dict(d));
		}
		cinja_temp_dict_set(d, temp_string_create("REPLIES"), replies);
		cinja_temp_dict_set(d, temp_string_create("comment"), comment_temp);
	}
	return d;
}


static cinja_list get_comments(art_root root, const string uri)
{
	cinja_list ls = art_get_comments(root, uri);
	if (!ls)
		return NULL;
	cinja_list comments = cinja_temp_list_create();
	for (size_t i = 0; i < ls->count; i++) {
		comment c = cinja_list_get(ls, ls->count - i - 1).item;
		cinja_list_add(comments, _comment_to_dict(c));
	}
	return comments;
}


/**
Query
*/

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
	size_t s = 4096, i = 0;
	string v = temp_alloc(sizeof(v->len) + s + 1);
	if (!v)
		return NULL;

	const char *p = ptr;
	for (v->len = 0; *ptr != delim && *ptr != 0; v->len++) {
		if (s <= i) {
			// TODO
			return NULL;
/*
			string tmp = realloc(v, s * 3 / 2);
			if (!tmp) {
				free(v);
				return NULL;
			}
			v = tmp;
			s = s * 3 / 2;
*/
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
	cinja_dict d = cinja_temp_dict_create();
	while (*ptr != 0) {
		string key = copy_query_field(&ptr, '=');
		string val = copy_query_field(&ptr, '&');
		cinja_temp_dict_set(d, key, val);
	}
	return d;
}


/**
Get the static file associated with a URI.

Returns: A valid response object with as body the contents of the static file.
*/
static response get_static_file(string uri)
{
	response r = response_create();
	string path;

	// Get the path to the requested file
	if (uri->len == 0) {
		path = temp_string_create("index.html");
		r->flags = RESPONSE_USE_TEMPLATE;
	} else {
		struct stat statbuf;
		string components[2] = { uri };
		if (stat(uri->buf, &statbuf) < 0) {
			components[1] = temp_string_create(".html");
			r->flags = RESPONSE_USE_TEMPLATE;
		} else if (S_ISDIR(statbuf.st_mode)) {
			components[1] = temp_string_create("/index.html");
			r->flags = RESPONSE_USE_TEMPLATE;
		} else {
			components[1] = temp_string_create("");
			r->flags = 0;
		}
		path = temp_string_concat(components, 2);
	}

	// Get the MIME type
	const string mime = get_mime_type(path);
	cinja_dict_set(r->headers, temp_string_create("Content-Type"), mime);

	// Load the file
	FILE *f = fopen(path->buf, "r");
	if (!f)
		return get_error_response(r, 500);
	fseek(f, 0, SEEK_END);
	size_t s = ftell(f);
	fseek(f, 0, SEEK_SET);
	r->body = temp_alloc(sizeof(r->body->len) + s + 1);
	fread(r->body->buf, s, 1, f);
	r->body->buf[s] = 0;
	fclose(f);
	r->body->len = s;

	r->status = 200;
	return r;
}


/**
Request handlers
*/

static response handle_post(const string uri)
{
	response r = response_create();
	if (strncmp("blog/", uri->buf, 5) != 0) {
		return get_error_response(r, 405);
	}
	if (uri->buf[5] == 0)
		return get_error_response(r, 405);
	cinja_list ls = art_get(blog_root, temp_string_create(uri->buf + 5));
	if (ls->count != 1) {
		cinja_list_free(ls);
		return get_error_response(r, 405);
	}

	char *body = temp_alloc(0xFFFF);
	size_t end = fread(body, 1, 0xFFFF, stdin);
	body[end] = 0;

	cinja_dict d = parse_query(body);
	comment c = temp_alloc(sizeof(*c));
	string val;

	val = cinja_dict_get(d, temp_string_create("author")).value;
	if (!val)
		return get_error_response(r, 400);
	c->author = temp_string_copy(val);

	val = cinja_dict_get(d, temp_string_create("body")).value;
	if (!val)
		return get_error_response(r, 400);
	c->body   = temp_string_copy(val);

	const char *rt_str = cinja_dict_get(d, temp_string_create("reply-to")).value;
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

	// Check if a blog post is requested
	if (strncmp("blog", uri->buf, 4) == 0 && (uri->buf[4] == '/' || uri->buf[4] == 0)) {
		response r = response_create();
		r->flags = RESPONSE_USE_TEMPLATE;

		// Cut the "blog" part of the uri
		const string nuri = temp_string_create(uri->buf + (uri->buf[4] == '/' ? 5 : 4));

		// Get the article(s)
		cinja_list arts = art_get(blog_root, nuri);
		if (!arts)
			return get_error_response(r, 404);

		if (arts->count == 1) {
			// If there is only one article, return the article itself
			cinja_dict d = cinja_temp_dict_create();
			article    a = cinja_list_get(arts, 0).item;
			if (set_article_dict(d, a, 1) < 0)
				return get_error_response(r, 500);
			if (a->prev != NULL) {
				cinja_dict_set(d, temp_string_create("PREV_URI"  ), temp_string_copy(a->prev->uri  ));
				cinja_dict_set(d, temp_string_create("PREV_TITLE"), temp_string_copy(a->prev->title));
			}
			if (a->next != NULL) {
				cinja_dict_set(d, temp_string_create("NEXT_URI"  ), temp_string_copy(a->next->uri  ));
				cinja_dict_set(d, temp_string_create("NEXT_TITLE"), temp_string_copy(a->next->title));
			}
			cinja_list comments = get_comments(blog_root, nuri);
			cinja_dict_set(d, temp_string_create("COMMENTS"), comments);
			cinja_dict_set(d, temp_string_create("comment" ), comment_temp);
			r->body  = cinja_temp_render(art_temp, d);
		} else {
			// Return the list of articles
			cinja_list dicts = cinja_temp_list_create();
			for (size_t i = 0; i < arts->count; i++) {
				cinja_dict d = cinja_temp_dict_create();
				article    c = cinja_list_get(arts, i).item;
				if (set_article_dict(d, c, 0) < 0)
					return get_error_response(r, 500);
				cinja_list_add(dicts, d);
			}
			cinja_dict dict = cinja_temp_dict_create();
			cinja_dict_set(dict, temp_string_create("ARTICLES"), dicts);
			r->body = cinja_temp_render(entry_temp, dict);
			char buf1[64], buf2[64];
			string bodystr = (void *)buf1, datestr = (void *)buf2;
			string_create("BODY", 4, bodystr);
			string_create("DATE", 4, datestr);
			for (size_t i = 0; i < dicts->count; i++) {
				cinja_dict d = dicts->items[i].item;
				free(cinja_dict_get(d, bodystr).value);
			}
		}
		r->status = 200;
		return r;
	} else {
		return get_static_file(uri);
	}
}


int main()
{
	// Setup
	temp_alloc_push(1 << 27);
	if (setup() < 0)
		return 1;
	temp_alloc_reset();

	// Loop
	while (FCGI_Accept() >= 0) {

		// Do not remove this header
		printf("X-My-Own-Header: All hail the mighty Duck God\r\n");

		// Get the request/FCGI variables
		const char *path_info  = getenv("PATH_INFO");
		const char *method = getenv("REQUEST_METHOD");
		if (!path_info || !method)
			RETURN_ERROR(1, "%s is not defined\n", !path_info ? "PATH_INFO" : "REQUEST_METHOD");

		// Redirect to HTTPS, if applicable
		if (redirect_tls) {
			const char *https = getenv("HTTPS");
			if (!https || strcmp(https, "on") != 0)
			{
				const char *host = getenv("HTTP_HOST");
				printf("Status: 301\r\n"
				       "Location: %s\r\n"
				       "\r\n"
				       "<a href=\"https://%s%s\">Click here to go to the secure page</a>",
				       path_info, host, path_info);
				continue;
			}
		}

		// Convert path_info to a string.
		if (path_info[0] == '/')
			path_info++;
		size_t path_info_l = strlen(path_info);
		if (path_info_l > 0 && path_info[path_info_l - 1] == '/')
			path_info_l--;
		string uri = temp_string_create(path_info, path_info_l);

		// Parse the request
		response r;
		if (strcmp(method, "GET") == 0)
			r = handle_get(uri);
		else if (strcmp(method, "POST") == 0)
			r = handle_post(uri);
		else
			r = get_error_response(response_create(), 501);

		// Convert the status integer to a string
		char status_str[64];
		snprintf(status_str, sizeof(status_str), "%d", r->status);

		// Check if the response should be wrapped in the base template
		if (r->flags & RESPONSE_USE_TEMPLATE) {
			cinja_dict d = cinja_temp_dict_create();
			cinja_dict_set(d, temp_string_create("BODY"), r->body);
			r->body = cinja_temp_render(main_temp, d);
			if (!r->body) {
				printf("Status: 500\r\nError during rendering");
				continue;
			}
		}

		// Pass the headers and body to the proxy
		void *state = NULL;
		printf("Status: %d\r\n", r->status);
		for (cinja_dict_entry_t e = cinja_dict_iter(r->headers, &state); e.value != NULL; e = cinja_dict_iter(r->headers, &state))
			printf("%s: %s\r\n", e.key->buf, ((string)e.value)->buf);
		printf("\r\n%s", r->body->buf);

		// Cleanup
		fflush(stdout);
		temp_alloc_reset();
	}

	temp_alloc_pop();
	art_free(blog_root);
	// Goddamnit Valgrind
	cinja_free(   main_temp);
	cinja_free(  error_temp);
	cinja_free(    art_temp);
	cinja_free(  entry_temp);
	cinja_free(comment_temp);
	return 0;
}
