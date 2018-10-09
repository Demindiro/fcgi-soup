#include <errno.h>
#include <fastcgi.h>
#include <fcgi_stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../include/template.h"
#include "../include/mime.h"


time_t container_mod_time;
time_t error_mod_time;
template container_temp;
template error_temp;
dictionary container_dict;
dictionary error_dict;


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
	       "Status: %d\r\n"
	       "\r\n"
	       "%s",
	       status, body);
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
				perror("Couldn't open 'error.phtml'");
				return -1;
			}
			char buf[statbuf.st_size + 1];
			if (read(fd, buf, statbuf.st_size) < 0)
				return -1;
			buf[statbuf.st_size] = 0;
			if (template_create(temp, buf) < 0)
				return -1;
			*time = statbuf.st_mtime;
		}
	} else {
		if (*time != 0) {
			template_free(temp);
			if (template_create(temp, def_temp) < 0)
				return -1;
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
	    dict_create(&container_dict) < 0)
		return -1;
	return check_templates();
}


int main()
{
	if (setup() < 0)
		return 1;
	char buf[0x10000];
	int pagesize = getpagesize();
	while (FCGI_Accept() >= 0) {
		check_templates();
		// Do not remove this header
		printf("X-My-Own-Header: All hail the mighty Duck God\r\n");
		
		char *uri = getenv("REQUEST_URI");
		if (uri == NULL)
			return 1;
		if (uri[0] == '/')
			uri++;
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

#ifndef NDEBUG
		printf("X-Debug-Date: " __DATE__ "\r\n");
		printf("X-Debug-Time: " __TIME__ "\r\n");
		printf("X-Debug-URI: %s\r\n", uri);
#endif
		int fd = open(uri, O_RDONLY);
		if (fd < 0) {
			print_error();
			continue;
		}
		
		const char *mime = get_mime_type(uri);
		if (mime != NULL)
			printf("Content-Type: %s\r\n", mime);
		printf("Status: 200\r\n"
		       "\r\n");
		char *body = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (body == NULL) {
			perror("Error during mapping");
			continue;
		}
		if (statbuf.st_size % pagesize == 0) {
			char *p = mmap(body + statbuf.st_size + 1, pagesize, PROT_WRITE | PROT_READ, MAP_FIXED | MAP_PRIVATE, -1, 0);
			p[0] = 0;
		}
		if (mime != NULL && strcmp(mime, "text/html") == 0) {
			if (dict_set(&container_dict, "BODY", body)) {
				perror("Error during setting BODY");
				continue;
			}
			munmap(body, statbuf.st_size);
			close(fd);
			body = template_parse(&container_temp, &container_dict);
			if (body == NULL) {
				perror("Error during parsing");
				continue;
			}
			if (printf("%*s", statbuf.st_size, body) < 0) {
				perror("Error during writing");
				continue;
			}
		} else {			
			if (printf("%*s", statbuf.st_size, body) < 0) {
				perror("Error during writing");
				continue;
			}
			munmap(body, statbuf.st_size + ((statbuf.st_size % pagesize == 0) ? 4096 : 0));
			close(fd);
		}
		fflush(stdout);
	}
	return 0;
}
