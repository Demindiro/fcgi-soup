#include "../include/article.h"
#include <dirent.h>
#include <stdlib.h>
#include <sys/dir.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "util/string.h"


static int to_int(const char **pptr, int *res)
{
	const char *ptr = *pptr;
	int num = 0;
	while (*ptr != '/' && *ptr != 0) {
		if (*ptr < '0' || '9' < *ptr)
			return -1;
		num *= 10;
		num += *ptr - '0';
	}
	*res  = num;
	*pptr = ptr;
	return 0;
}


static int get_month_length(int year, int month)
{
	static int l[12] = {31,28,31,30,
	                    31,30,31,31,
	                    30,31,30,31};
	if (month == 2 && year % 4 == 0 && year % 100 != 0)
		return 29;
	return l[month-1];
}


static char *get_between_times(article_root *root, time_t t0, time_t t1)
{
	size_t size = 65536, len = 0;
	char *buf = malloc(size);
	for (size_t i = 0; i < root->count; i++) {
		time_t t = root->articles[i].date;
		if (t0 <= t && t <= t1) {
			char  *str  = root->articles[i].name;
			size_t strl = strlen(str);
			if (buf_write(&buf, &len, &size, root->articles[i].name, strl + 1) < 0) {
				free(buf);
				return NULL;
			}
			buf[len++] = '\n';
		}
	}
	if (buf_write(&buf, &len, &size, "", 1) < 0) {
		free(buf);
		return NULL;
	}
	buf = realloc(buf, len);
	return buf;
}


static char *article_get_by_time(article_root *root, time_t time)
{
	const char *name;
	char *buf;
	int fd;
	for (size_t i = 0; i < root->count; i++) {
		time_t t = root->articles[i].date;
		if (time == t) {
			name = root->articles[i].name;
			goto found;
		}
	}
	return NULL;
found:
	fd = open(name, O_RDONLY);
	if (fd < 0)
		return NULL;
	struct stat statbuf;
	fstat(fd, &statbuf);
	buf = malloc(statbuf.st_size + 1);
	read(fd, buf, statbuf.st_size);
	close(fd);
	buf[statbuf.st_size] = 0;
	return buf;
}


int article_init(article_root *root, const char *path)
{
	DIR *dir = opendir(path);
	if (dir == NULL)
		return -1;
	root->root  = string_copy(path);
	root->size  = 16;
	root->count = 0;
	root->articles = malloc(root->size * sizeof(*root->articles));
	for (struct dirent *de = readdir(dir); de != NULL; de = readdir(dir), root->count++) {
		if (!(de->d_type == DT_REG || de->d_type == DT_UNKNOWN))
			continue;
		root->articles[root->count].name = string_copy(de->d_name);
		if (root->articles[root->count].name == NULL)
			goto error;
	}
	closedir(dir);
	return 0;
error:
	closedir(dir);
	for (size_t i = 0; i < root->count; i++)
		free(root->articles[i].name);
	free(root->articles);
	free(root->root);
	return -1;
}


const char *article_get(article_root *root, const char *uri) {
	const char *ptr = uri;
	struct tm date;
	time_t time0, time1;
	int year, month, day;
	char *buf;
	if ('0' <= *ptr && *ptr <= '9') {
		memset(&date, 0, sizeof(date));
		if (to_int(&ptr, &year ) < 0 || !(ptr++) || // !x if x != 0 --> 0
		    to_int(&ptr, &month) < 0 || !(ptr++) ||
		    to_int(&ptr, &day  ) < 0)
			return NULL;
		date.tm_year = year  - 1900;
		if (month == 0) {
			date.tm_mon  = 0;
			date.tm_mday = 1;
			time0 = mktime(&date);
			date.tm_mon  = 11;
			date.tm_mday = 31;
			time1 = mktime(&date);
			buf = get_between_times(root, time0, time1);
		} else if (day == 0) {
			date.tm_mon  = month - 1;
			date.tm_mday = 1;
			time0 = mktime(&date);
			date.tm_mday = get_month_length(year, month);
			time1 = mktime(&date);
			buf = get_between_times(root, time0, time1);
		} else {
			date.tm_mon  = month - 1;
			date.tm_mday = day;
			time0 = mktime(&date);
			buf = article_get_by_time(root, time0);
		}
	} else if (*ptr == 0) {
		
	} else {

	}
	return NULL;
}
