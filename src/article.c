#include "../include/article.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <dirent.h>
#include "util/string.h"

#define GET_MMAP   0x1
#define GET_MALLOC 0x2


static int to_int(const char **pptr, int *res)
{
	char *ptr = *pptr;
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


static char *get_between_times(article_root *root, time_t t0, time_t t1, int *code)
{
	size_t size = 65536, len = 0;
	char *buf = malloc(size);
	for (size_t i = 0; i < root->count; i++) {
		time_t t = root->stats[i].st_time;
		if (t0 <= t && t <= t1) {
			char  *str  = root->names[i];
			size_t strl = strlen(str);
			if (buf_write(&buf, &len, &size, root->names[i], strl + 1) < 0) {
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
	*code = GET_MALLOC;
	return buf;
}


static char *mmap_of_time(article_root *root, time_t time, int code)
{
	const char *name;
	for (size_t i = 0; i < root->count; i++) {
		time_t t = root->stats[i].st_time;
		if (time == t) {
			name = root->names[i];
			goto found;
		}
	}
	return NULL;
found:
	int   fd  = open();
	char *buf = mmap();
	*code = GET_MALLOC;
	return buf;
}


int article_init(article_root *root, const char *path)
{
	DIR *dir = opendir(path);
	if (dir == NULL)
		return -1;
	root->root  = copy_string(path);
	root->size  = 16;
	root->count = 0;
	root->names = malloc(root->size * sizeof(*root->names));
	root->stats = malloc(root->size * sizeof(*root->stats));
	for (dirent *de = readdir(dir); de != NULL; de = readdir(dir), root->count++) {
		if (!(de->d_type == DT_FILE || de->d_type == DT_UNKNOWN))
			continue;
		if (stat(de->d_name, &root->stats[root->count]) < 0)
			goto error;
		root->names[root->count] = copy_string(de->d_name);
		if (root->names[root->count] == NULL)
			goto error;
	}
	closedir(dir);
	return 0;
error:
	for (size_t i = 0; i < root->count; i++)
		free(root->names[i]);
	free(root->names);
	free(root->stats);
	free(root->root);
	return -1;
}


const char *article_get(article_root *root, const char *uri, int *code) {
	const char *ptr = uri;
	struct tm date;
	time_t time0, time1;
	int year, month, day;
	char *buf;
	if ('0' <= *ptr && *ptr <= '9') {
		memset(date, 0, sizeof(date));
		if (to_int(&(  ptr), &year ) < 0 ||
		    to_int(&(++ptr), &month) < 0 ||
		    to_int(&(++ptr), &day  ) < 0)
			return -1;
		date.tm_year = year  - 1900;
		if (month == 0) {
			date.tm_mon  = 0;
			date.tm_mday = 1;
			time0 = mktime(date);
			date.tm_mon  = 11;
			date.tm_mday = 31;
			time1 = mktime(date);
			buf = get_between_times(root, time0, time1, code);
		} else if (day == 0) {
			date.tm_mon  = month - 1;
			date.tm_mday = 1;
			time0 = mktime(date);
			date.tm_mday = get_month_length(year, month);
			time1 = mktime(date);
			buf = get_between_times(root, time0, time1, code);
		} else {
			date.tm_mon  = month - 1;
			date.tm_mday = day;
			time0 = mktime(date);
			buf = mmap_of_time(root, time0, code);
		}
	} else if (*ptr == 0) {
		
	} else {

	}
}

void article_free_get(char *ptr, int code) {
	switch(code) {
		case GET_MMAP:
			munmap(ptr);
			break;
		case GET_MALLOC:
			free(ptr);
			break;
	}
}
