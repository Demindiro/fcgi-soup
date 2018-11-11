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


static int print_error() {
	char *status;
	char *msg;
	switch(errno) {
	case ENOENT:
		status = "404";
		msg = "File not found";
		break;
	case EACCES:
		status = "403";
		msg = "Forbidden";
		break;
	default:
		status = "500";
		msg = strerror(errno);
	}

	dict d = dict_create();
	dict_set(d, "STATUS", status);
	dict_set(d, "MESSAGE", msg);

	char *body = temp_render(error_temp, d);
	if (body == NULL)
		return -1;
	dict_set(d, "BODY", body);
	free(body);
	body = temp_render(main_temp, d);
	if (body == NULL)
		return -1;
	printf("Content-Type: text/html\r\n"
	       "Content-Length: %d\r\n"
	       "Status: %s\r\n"
	       "\r\n"
	       "%s",
	       strlen(body), status, body);
	free(body);
	dict_free(d);
	fflush(stdout);
	return 0;
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


int main()
{
	if (setup() < 0)
		return 1;

	char buf[0x10000];
	while (FCGI_Accept() >= 0) {	
		// Do not remove this header
		printf("X-My-Own-Header: All hail the mighty Duck God\r\n");
		
		char *uri = getenv("PATH_INFO");
		if (uri == NULL)
			return 1;
		if (uri[0] == '/')
			uri++;

		dict md = dict_create();

		if (strncmp("blog", uri, 4) == 0 && (uri[4] == '/' || uri[4] == 0)) {
			char *nuri = uri + (uri[4] == '/' ? 5 : 4);
			list arts = art_get(blog_root, nuri);
			if (arts == NULL) {
				print_error();
				continue;
			}
			char *body;
			dict d = dict_create();
			if (arts->count == 1) {
				article a;
				list_get(arts, 0, &a);
				if (set_art_dict(d, a, 0x1) < 0) {
					print_error();
					continue;
				}
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
				body = temp_render(art_temp, d);
			} else {
				size_t size = 0x1000, index = 0;
				body = malloc(size);
				for (size_t i = 0; i < arts->count; i++) {
					article c;
					list_get(arts, i, &c);
					if (set_art_dict(d, c, 0) < 0) {
						print_error();
						goto error;
					}
					char *buf = temp_render(entry_temp, d);
					if (buf == NULL) {
						print_error();
						dict_free(d);
						goto error;
					}
					if (buf_write(&body, &index, &size, buf, strlen(buf)) < 0)
						/* TODO */;
					free(buf);
				}
				char nul[1] = { 0 };
				if (buf_write(&body, &index, &size, nul, 1) < 0)
					/* TODO */;
			}
			dict_free(d);
			list_free(arts);
			dict_set(md, "BODY", body);
			free(body);
		} else {
			struct stat statbuf;
			if (uri[0] == 0)
				uri = "index.html";
			if (stat(uri, &statbuf) < 0) {	
				print_error();
				continue;
			}
			if (S_ISDIR(statbuf.st_mode)) {
				size_t l = strlen(uri);
				memcpy(buf, uri, l);
				memcpy(buf + l, "/index.html", sizeof("/index.html"));
				uri = buf;
			}

			int fd = open(uri, O_RDONLY);
			if (fd < 0) {
				print_error();
				continue;
			}
			const char *mime = get_mime_type(uri);
			if (mime != NULL)
				printf("Content-Type: %s\r\n", mime);
			char *body = file_read(fd, statbuf.st_size);
			if (body == NULL) {
				perror("Error during reading");
				continue;
			}
			if (mime != NULL && strcmp(mime, "text/html") == 0) {
				if (dict_set(md, "BODY", body)) {
					perror("Error during setting BODY");
					free(body);
					continue;
				}
				free(body);
			} else {			
				if (printf("%*s", statbuf.st_size, body) < 0) {
					perror("Error during writing");
					free(body);
					continue;
				}
				free(body);
				continue;
			}
		}
		char *body = temp_render(main_temp, md);
		if (body == NULL) {
			perror("Error during parsing");
			continue;
		}
		if (printf("Status: 200\r\n\r\n%s", body) < 0) {
			perror("Error during writing");
			free(body);
			continue;
		}
	error:
		free(body);
		dict_free(md);
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
