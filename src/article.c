/*
 * Database format:
 *   -  64 bytes for the uri
 *   - 256 bytes for the name
 *   -   4 bytes for the date
 *     - 12 bits for the year        (covers 4096 years)
 *     -  4 bits for the month       (covers 12 months) *     -  5 bits for the day         (covers 31 days)
 *     - 11 bits for the time of day (covers 1440 minutes)
 *   -  64 bytes for the author's name
 *   -  64 bytes for the title
 */

#include "../include/article.h"
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h> // htonl()
#include "util/string.h"
#include "../include/database.h"
#include "../include/dictionary.h"
#include "../include/template.h"


#define DEF_TEMP_BODY "<h1>{TITLE}</h1><article>{BODY}</article>"


typedef unsigned int uint;


static template temp;


__attribute__((constructor))
static void init()
{
	char *body = NULL;
	int fd = open("article.phtml", O_RDONLY);
	if (fd < 0) {
		template_create(&temp, DEF_TEMP_BODY);
	} else {
		struct stat statbuf;
		if (fstat(fd, &statbuf) < 0) {
			close(fd);
			template_create(&temp, DEF_TEMP_BODY);
		} else {
			body = malloc(statbuf.st_size + 1);
			if (body == NULL || read(fd, body, statbuf.st_size) != statbuf.st_size) {
				close(fd);
				free(body);
				template_create(&temp, DEF_TEMP_BODY);
				return;
			}
			close(fd);
			body[statbuf.st_size] = 0;
			template_create(&temp, body);
		}
	}
}


uint32_t format_date(uint year, uint month, uint day, uint hour, uint minute)
{
	uint time = hour * 60 + minute;
	uint32_t date = 0;
	date += (time  & ((1 << 11) - 1)) <<  0;
	date += (day   & ((1 <<  5) - 1)) << 11;
	date += (month & ((1 <<  4) - 1)) << 16;
	date += (year  & ((1 << 12) - 1)) << 20;
	return date;
}


int date_to_str(char *buf, uint32_t date)
{
	uint year   = (date >> 20) & ((1 << 12) - 1);
	uint month  = (date >> 16) & ((1 <<  4) - 1);
	uint day    = (date >> 11) & ((1 <<  5) - 1);
	uint time   = (date >>  0) & ((1 << 11) - 1);
	uint hour   = time / 60;
	uint minute = time % 60;
	return sprintf(buf, "%02u-%02u-%02u %02u:%02u", year, month, day, hour, minute);
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
		ptr++;
	}
	*res  = num;
	*pptr = ptr;
	return 0;
}


static char *article_get_between_times(article_root *root, uint32_t t0, uint32_t t1)
{
	t0 = htonl(t0);
	t1 = htonl(t1);
	size_t size = 65536, len = 0;
	char *buf = malloc(size);
	const char **entries;
	size_t count = database_get_range(&root->db, &entries, DB_DATE_FIELD, &t0, &t1);
	for (size_t i = 0; i < count; i++) {
		char uri[DB_URI_LEN + 1], title[DB_TITLE_LEN + 1];
		database_get_field(&root->db, uri  , entries[i], DB_URI_FIELD  );
		database_get_field(&root->db, title, entries[i], DB_TITLE_FIELD);
		uri  [DB_URI_LEN  ] = 0;
		title[DB_TITLE_LEN] = 0;
		char link[512];
		ssize_t l = sprintf(link, "<a href=\"%s\">%s</a><br>", uri, title);
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
	size_t l = strlen(path);	
	
	int append_slash = path[l-1] != '/';
	root->dir = malloc(l + append_slash + 1);
	if (root->dir == NULL)
		return -1;
	memcpy(root->dir, path, l);
	if (append_slash) {
		root->dir[l+0] = '/';
		root->dir[l+1] = 0;
	} else {
		root->dir[l] = 0;
	}

	char buf[256];
	memcpy(buf, path, l);
	memcpy(buf + l, ".db", sizeof(".db"));
	if (database_load(&root->db, buf) < 0) {
		uint8_t f_count = 5;
		uint16_t f_lens[5] = {
			[DB_URI_FIELD   ] = DB_URI_LEN   ,
			[DB_FILE_FIELD  ] = DB_FILE_LEN  ,
			[DB_DATE_FIELD  ] = DB_DATE_LEN  ,
			[DB_AUTHOR_FIELD] = DB_AUTHOR_LEN,
			[DB_TITLE_FIELD ] = DB_TITLE_LEN ,
		}; 
		if (database_create(&root->db, buf, f_count, f_lens) < 0) {
			free(root->dir);
			return -1;
		}
		database_create_map(&root->db, DB_URI_FIELD);
		database_create_map(&root->db, DB_DATE_FIELD);
	}
	return 0;
}


const char *article_get(article_root *root, const char *uri) {
	const char *ptr = uri;
	if ('0' <= *ptr && *ptr <= '9') {
		uint year = 0, month = 0, day = 0;
		uint32_t time0, time1;
		if (to_uint(&ptr, &year ) < 0)
			return NULL;
                if (*ptr != 0 && ptr++ && to_uint(&ptr, &month) < 0)
			return NULL;
		if (*ptr != 0 && ptr++ && to_uint(&ptr, &day  ) < 0)
			return NULL;
		if (month == 0) {
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
		char dburi[DB_FILE_LEN];
		memset(dburi, 0, sizeof(dburi));
		strncpy(dburi, uri, sizeof(dburi));
		
		const char *entry = database_get(&root->db, DB_URI_FIELD, dburi);
		if (entry == NULL)
			return NULL;

		char file[512], *ptr = file;
		size_t l = strlen(root->dir);
		memcpy(ptr, root->dir, l);
		ptr += l;
		if (database_get_field(&root->db, ptr, entry, DB_FILE_FIELD) < 0)
			return NULL;
		ptr[strlen(ptr)] = 0;

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
			close(fd);
			free(buf);
			return NULL;
		}
		close(fd);
		buf[statbuf.st_size] = 0;
		
		const char *prev_entry = database_get_offset(&root->db, DB_URI_FIELD, dburi, -1);
		const char *next_entry = database_get_offset(&root->db, DB_URI_FIELD, dburi,  1);

		uint32_t date;
		char title[DB_TITLE_LEN], author[DB_AUTHOR_LEN], datestr[64],
		     prev[DB_URI_LEN], next[DB_URI_LEN];
		if (database_get_field(&root->db, (char *)&date, entry, DB_DATE_FIELD  ) < 0 ||
		    database_get_field(&root->db,  title , entry, DB_TITLE_FIELD ) < 0 ||
		    database_get_field(&root->db,  author, entry, DB_AUTHOR_FIELD) < 0 ||
		    date_to_str(datestr, htonl(date)) < 0 || // memcmp is effectively big-endian
		    (prev_entry != NULL && database_get_field(&root->db, prev, prev_entry,
		                                              DB_URI_FIELD)) ||
		    (next_entry != NULL && database_get_field(&root->db, next, next_entry,
		                                              DB_URI_FIELD)))
			/* TODO */;

		dictionary dict;
		dict_create(&dict);
		dict_set(&dict, "BODY"    , buf    );
		dict_set(&dict, "TITLE"   , title  );
		dict_set(&dict, "AUTHOR"  , author );
		dict_set(&dict, "DATE"    , datestr);
		if (prev_entry != NULL)
			dict_set(&dict, "PREVIOUS", prev);
		if (next_entry != NULL)
			dict_set(&dict, "NEXT"    , next);
		char *body = template_parse(&temp, &dict);
		dict_free(&dict);
		free(buf);
		return body;
	}
	return NULL;
}

void article_free(article_root *root)
{
	database_free(&root->db);
	free(root->dir);
}
