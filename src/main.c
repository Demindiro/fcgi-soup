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
#include "../include/art.h"
#include "../include/dict.h"
#include "../include/template.h"
#include "../include/mime.h"


#define TEMPLATE_DIR "templates/"
#define MAIN_TEMP    TEMPLATE_DIR "main.html"
#define ERROR_TEMP   TEMPLATE_DIR "error.html"
#define ARTICLE_TEMP TEMPLATE_DIR "article.html"
#define ENTRY_TEMP   TEMPLATE_DIR "article_entry.html"
#define COMMENT_TEMP TEMPLATE_DIR "comment.html"


time_t main_mod_time;
time_t error_mod_time;
template main_temp;
template error_temp;
template art_temp;
template entry_temp;
template comment_temp;
dictionary main_dict;
dictionary error_dict;

art_root blog_root;


#define return_error(msg, ...) { fprintf(stderr, msg, ##__VA_ARGS__); perror(": "); return -1; } ""


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
	dict_set(&error_dict, "STATUS", status);
	dict_set(&error_dict, "MESSAGE", msg);

	char *body = template_parse(&error_temp, &error_dict);
	if (body == NULL)
		return -1;
	dict_set(&main_dict, "BODY", body);
	free(body);
	body = template_parse(&main_temp, &main_dict);
	if (body == NULL)
		return -1;
	printf("Content-Type: text/html\r\n"
	       "Content-Length: %d\r\n"
	       "Status: %s\r\n"
	       "\r\n"
	       "%s",
	       strlen(body), status, body);
	free(body);
	fflush(stdout);
	return 0;
}


static int load_template(char *file, template *temp)
{
	int fd = open(file, O_RDONLY);
	if (fd < 0)
		return_error("Couldn't open '%s'", file);
	struct stat statbuf;
	if (fstat(fd, &statbuf) < 0)
		return_error("Couldn't get size of '%s'", file);
	char buf[statbuf.st_size + 1];
	if (read(fd, buf, statbuf.st_size) < 0)
		return_error("Couldn't read '%s'", file);
	buf[statbuf.st_size] = 0;
	if (template_create(temp, buf) < 0)
		return_error("Couldn't create template of '%s'", file);
	return 0;
}


static int setup()
{
	if (dict_create(&error_dict    ) < 0 ||
	    dict_create(&main_dict) < 0) {
		perror("Couldn't create dictionaries");
		return -1;
	}
	if (load_template(MAIN_TEMP   , &main_temp   ) < 0 ||
	    load_template(ERROR_TEMP  , &error_temp  ) < 0 ||
	    load_template(ARTICLE_TEMP, &art_temp    ) < 0 ||
	    load_template(COMMENT_TEMP, &comment_temp) < 0 ||
	    load_template(ENTRY_TEMP  , &entry_temp  ) < 0)
		return -1;
	return art_init(&blog_root, "blog");
}


static char *map_or_read_file(int fd, size_t size)
{
	char *buf;
	if (size % getpagesize() > 0) {
		buf = mmap(NULL, size, PROT_READ | PROT_WRITE , MAP_PRIVATE, fd, 0);
	} else {
		buf = malloc(size + 1);
		read(fd, buf, size);
	}
	buf[size] = 0;
	close(fd);
	return buf;
}

static void unmap_or_free_file(char *ptr, size_t size)
{
	if (size % getpagesize() > 0)
		munmap(ptr, size);
	else
		free(ptr);
}


static int set_art_dict(dictionary *dict, article *art, int flags) {
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
		dict_set(dict, "BODY", buf);
		free(buf);
	}

	char datestr[64];
	date_to_str(datestr, art->date);

	dict_set(dict, "URI"   , art->uri   );
	dict_set(dict, "DATE"  , datestr    );
	dict_set(dict, "TITLE" , art->title );
	dict_set(dict, "AUTHOR", art->author);

	return 0;
}

static char *render_comment(art_comment *comment)
{
	dictionary d;
	char buf[64];
	date_to_str(buf, comment->date);
	dict_create(&d);
	dict_set(&d, "AUTHOR", comment->author);
	dict_set(&d, "DATE"  , buf);
	dict_set(&d, "BODY"  , comment->body);
	if (comment->replies->count > 0) {
		size_t size = 256, index = 0;
		char *buf = malloc(size);
		for (size_t i = 0; i < comment->replies->count; i++) {
			char *b = render_comment((art_comment *)list_get(comment->replies, i));
			buf_write(&buf, &index, &size, b, strlen(b));
			free(b);
		}
		dict_set(&d, "REPLIES", buf);
	}
	char *body = template_parse(&comment_temp, &d);
	dict_free(&d);
	return body;
}


static char *get_comments(art_root *root, const char *uri)
{
	list ls = art_get_comments(root, uri);
	if (ls == NULL)
		return NULL;
	size_t size = 256, index = 0;
	char *buf = malloc(size);
	for (size_t i = 0; i < ls->count; i++) {
		char *b = render_comment((art_comment *)list_get(ls, i));
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

		if (strncmp("blog", uri, 4) == 0 && (uri[4] == '/' || uri[4] == 0)) {
			char *nuri = uri + (uri[4] == '/' ? 5 : 4);
			article *arts;
			size_t count = art_get(&blog_root, &arts, nuri);
			if (count == -1) {
				print_error();
				continue;
			}
			char *body;
			dictionary dict;
			dict_create(&dict);
			if (count == 1) {
				if (set_art_dict(&dict, &arts[0], 0x1) < 0) {
					print_error();
					continue;
				}
				article *art;
				if (arts[0].prev[0] != 0) {
					art_get(&blog_root, &art, arts[0].prev);
					dict_set(&dict, "PREV_URI"  , art->uri  );
					dict_set(&dict, "PREV_TITLE", art->title);
					free(art);
				}
				if (arts[0].next[0] != 0) {
					art_get(&blog_root, &art, arts[0].next);
					dict_set(&dict, "NEXT_URI"  , art->uri  );
					dict_set(&dict, "NEXT_TITLE", art->title);
					free(art);
				}
				char *b = get_comments(&blog_root, nuri);
				dict_set(&dict, "COMMENTS", b);
				free(b);
				body = template_parse(&art_temp, &dict);
			} else {
				size_t size = 0x1000, index = 0;
				body = malloc(size);
				for (size_t i = 0; i < count; i++) {
					if (set_art_dict(&dict, &arts[i], 0) < 0) {
						print_error();
						goto error;
					}
					char *buf = template_parse(&entry_temp, &dict);
					if (buf == NULL) {
						print_error();
						dict_free(&dict);
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
			dict_free(&dict);
			dict_set(&main_dict, "BODY", body);
			free(body);
			free(arts);
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
			char *body = map_or_read_file(fd, statbuf.st_size);
			if (body == NULL) {
				perror("Error during mapping");
				continue;
			}
			if (mime != NULL && strcmp(mime, "text/html") == 0) {
				if (dict_set(&main_dict, "BODY", body)) {
					perror("Error during setting BODY");
					unmap_or_free_file(body, statbuf.st_size);
					continue;
				}
				unmap_or_free_file(body, statbuf.st_size);
			} else {			
				if (printf("%*s", statbuf.st_size, body) < 0) {
					perror("Error during writing");
					unmap_or_free_file(body, statbuf.st_size);
					continue;
				}
				unmap_or_free_file(body, statbuf.st_size);
				continue;
			}
		}
		char *body = template_parse(&main_temp, &main_dict);
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
		fflush(stdout);
	}
	art_free(&blog_root);
	return 0;
}
