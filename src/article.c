/*
 * Database format:
 *   - 256 bytes for the name
 *   -   4 bytes for the date
 *     - 12 bits for the year        (covers 4096 years)
 *     -  4 bits for the month       (covers 12 months)
 *     -  5 bits for the day         (covers 31 days)
 *     - 11 bits for the time of day (covers 1440 minutes)
 *   -  64 bytes for the author's name
 *   -  64 bytes for the title
 */

#include "../include/article.h"
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/dir.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "util/string.h"
#include "../include/database.h"


#define DB_NAME_LEN     256
#define DB_DATE_LEN     4
#define DB_AUTHOR_LEN   64
#define DB_TITLE_LEN    64

#define DB_NAME_FIELD   0
#define DB_DATE_FIELD   1
#define DB_AUTHOR_FIELD 2
#define DB_TITLE_FIELD  3


typedef unsigned int uint;


static uint32_t format_date(uint year, uint month, uint day, uint hour, uint minute) {
	uint time = hour * 60 + minute;
	uint32_t date = 0;
	date += (time  & ((1 << 11) - 1)) <<  0;
	date += (day   & ((1 <<  5) - 1)) << 11;
	date += (month & ((1 <<  4) - 1)) << 16;
	date += (year  & ((1 << 12) - 1)) << 20;
	return date;
}


static int to_uint(const char **pptr, uint *res)
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


static char *article_get_between_times(article_root *root, uint32_t t0, uint32_t t1)
{
	size_t size = 65536, len = 0;
	char *buf = malloc(size);
	const char **entries;
	size_t count = database_get_range(&root->db, &entries, DB_DATE_FIELD, &t0, &t1);
	for (size_t i = 0; i < count; i++) {
		char name[DB_NAME_LEN + 1], title[DB_TITLE_LEN + 1];
		database_get_field(&root->db, name, entries[i], DB_NAME_FIELD);
		database_get_field(&root->db, name, entries[i], DB_TITLE_FIELD);
		name [DB_NAME_LEN ] = 0;
		title[DB_TITLE_LEN] = 0;
		char link[512];
		ssize_t l = sprintf(link, "<a href=\"%s\">%s</a><br>", name, title);
		if (l < 0 || buf_write(&buf, &len, &size, link, l) < 0) {
			free(buf);
			return NULL;
		}
	}
	free(entries);
	if (buf_write(&buf, &len, &size, "", 1) < 0) {
		free(buf);
		return NULL;
	}
	buf = realloc(buf, len);
	return buf;
}


int article_init(article_root *root, const char *path)
{
	char buf[256];
	size_t l = strlen(path);
	root->dir = malloc(l + 1);
	if (root->dir == NULL)
		return -1;
	
	memcpy(buf, path, l);
	memcpy(buf + l, ".db", sizeof(".db"));
	if (database_load(&root->db, buf) < 0) {
		uint8_t f_count = 4;
		uint16_t f_lens[4] = {
			[DB_NAME_FIELD  ] = DB_NAME_LEN  ,
			[DB_DATE_FIELD  ] = DB_DATE_LEN  ,
			[DB_AUTHOR_FIELD] = DB_AUTHOR_LEN,
			[DB_TITLE_FIELD ] = DB_TITLE_LEN ,
		}; 
		if (database_create(&root->db, buf, f_count, f_lens) < 0) {
			free(root->dir);
			return -1;
		}
	}
	return 0;
}


const char *article_get(article_root *root, const char *uri) {
	const char *ptr = uri;
	if ('0' <= *ptr && *ptr <= '9') {
		uint year, month, day;
		uint32_t time0, time1;
		if (to_uint(&ptr, &year ) < 0 || !(ptr++) || // !x if x != 0 --> 0
		    to_uint(&ptr, &month) < 0 || !(ptr++) ||
		    to_uint(&ptr, &day  ) < 0)
			return NULL;
		if (year == 0) {
			time0 = 0;
			time1 = 0xFFffFFff;
		} else if (month == 0) {
			time0 = format_date(year,  0,  0,  0,  0);
			time1 = format_date(year, 11, 31, 23, 59);
		} else if (day == 0) {
			time0 = format_date(year, month,  0,  0,  0);
			time1 = format_date(year, month, 31, 23, 59);
		} else {
			time0 = format_date(year, month, day,  0,  0);
			time1 = format_date(year, month, day, 23, 59);
		}
		return article_get_between_times(root, time0, time1);
	} else if (*ptr == 0) {
		return article_get_between_times(root, 0, 0xFFffFFff);	
	} else {
		char file[512], *ptr = file;
		size_t l = strlen(root->dir);
		memcpy(ptr, root->dir, l);
		ptr += l;
		*(ptr++) = '/';
		l = strlen(uri);
		memcpy(ptr, uri, l);
		ptr += l;
		*(ptr++) = '.';
		*(ptr++) = 'm';
		*(ptr++) = 'd';
		*ptr = 0;
		int fd = open(file, O_RDONLY);
		if (fd < 0)
			return NULL;
		struct stat statbuf;
		if (fstat(fd, &statbuf) < 0) {
			close(fd);
			return NULL;
		}
		char *buf = malloc(statbuf.st_size + 1);
		if (read(fd, buf, statbuf.st_size) != statbuf.st_size) {
			free(buf);
			buf = NULL;
		} else {
			buf[statbuf.st_size] = 0;
		}
		close(fd);
		return buf;
	}
	return NULL;
}
