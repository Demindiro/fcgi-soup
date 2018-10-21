#include <errno.h>
#include <fastcgi.h>
#include <fcgi_stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../include/article.h"
#include "../include/dictionary.h"
#include "../include/template.h"
#include "../include/mime.h"


time_t container_mod_time;
time_t error_mod_time;
template container_temp;
template error_temp;
dictionary container_dict;
dictionary error_dict;

article_root blog_root;


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
	dict_set(&container_dict, "BODY", body);
	free(body);
	body = template_parse(&container_temp, &container_dict);
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


static int check_template(char *file, time_t *time, template *temp, const char *def_temp)
{
	struct stat statbuf;
	if (stat(file, &statbuf) >= 0 && !S_ISDIR(statbuf.st_mode)) {	
		if (*time != statbuf.st_mtime) {
			template_free(temp);
			int fd = open(file, O_RDONLY);
			if (fd < 0) {
				perror("Couldn't open file");
				return -1;
			}
			char buf[statbuf.st_size + 1];
			if (read(fd, buf, statbuf.st_size) < 0) {
				perror("Couldn't read file");
				return -1;
			}
			buf[statbuf.st_size] = 0;
			if (template_create(temp, buf) < 0) {
				perror("Couldn't create template of file");
				return -1;
			}
			*time = statbuf.st_mtime;
		}
	} else {
		if (*time != 0) {
			template_free(temp);
			if (template_create(temp, def_temp) < 0) {
				perror("Couldn't create base template");
				return -1;
			}
			*time = 0;
		}
	}
	return 0;
}


static int check_templates()
{
	if (check_template("error.phtml"    ,     &error_mod_time,     &error_temp, "<h1>{STATUS}: {MESSAGE}</h1>"      ) < 0 || 
	    check_template("container.phtml", &container_mod_time, &container_temp, "<!DOCTYPE html><body>{BODY}</body>") < 0)
		return -1;
	return 0;
}


static int setup()
{
	if (dict_create(&error_dict    ) < 0 ||
	    dict_create(&container_dict) < 0) {
		perror("Couldn't create dictionaries");
		return -1;
	}
	if (check_templates() < 0)
		return -1;
	return article_init(&blog_root, "blog");
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

int main()
{
	if (setup() < 0)
		return 1;
	char buf[0x10000];
	while (FCGI_Accept() >= 0) {	
		check_templates();

		// Do not remove this header
		printf("X-My-Own-Header: All hail the mighty Duck God\r\n");
		
		char *uri = getenv("REQUEST_URI");
		if (uri == NULL)
			return 1;
		if (uri[0] == '/')
			uri++;

		if (strncmp("blog/", uri, 5) == 0) {
			char *nuri = uri + 5;
			const char *body = article_get(&blog_root, nuri);
			if (body == NULL) {
				print_error();
				continue;
			}
			dict_set(&container_dict, "BODY", body);
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
				if (dict_set(&container_dict, "BODY", body)) {
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
		char *body = template_parse(&container_temp, &container_dict);
		if (body == NULL) {
			perror("Error during parsing");
			continue;
		}
		if (printf("Status: 200\r\n\r\n%s", body) < 0) {
			perror("Error during writing");
			free(body);
			continue;
		}
		free(body);
		fflush(stdout);
	}
	return 0;
}
