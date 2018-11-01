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
#include <sys/dir.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "util/string.h"
#include "../include/database.h"
#include "../include/dictionary.h"
#include "../include/template.h"



typedef unsigned int uint;



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
	return sprintf(buf, "%u-%u-%u %u:%u", year, month, day, hour, minute);
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


static int article_get_between_times(article_root *root, article **dest, uint32_t t0, uint32_t t1)
{
	const char **entries;
	size_t count = database_get_range(&root->db, &entries, ARTICLE_DATE_FIELD, &t0, &t1);
	article *arts = *dest = calloc(count, sizeof(article));
	for (size_t i = 0; i < count; i++) {
		database_get_field(&root->db, arts[i].uri   , entries[i], ARTICLE_URI_FIELD   );
		database_get_field(&root->db, arts[i].file  , entries[i], ARTICLE_FILE_FIELD  );
		database_get_field(&root->db, (char *)&arts[i].date  , entries[i], ARTICLE_DATE_FIELD  );
		database_get_field(&root->db, arts[i].author, entries[i], ARTICLE_AUTHOR_FIELD);
		database_get_field(&root->db, arts[i].title , entries[i], ARTICLE_TITLE_FIELD );
	}
	free(entries);
	return count;
}


int article_init(article_root *root, const char *path)
{
	size_t l = strlen(path);	
	
	int append_slash = path[l-1] != '/';
	root->dir = malloc(l + append_slash);
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
			[ARTICLE_URI_FIELD   ] = ARTICLE_URI_LEN   ,
			[ARTICLE_FILE_FIELD  ] = ARTICLE_FILE_LEN  ,
			[ARTICLE_DATE_FIELD  ] = ARTICLE_DATE_LEN  ,
			[ARTICLE_AUTHOR_FIELD] = ARTICLE_AUTHOR_LEN,
			[ARTICLE_TITLE_FIELD ] = ARTICLE_TITLE_LEN ,
		}; 
		if (database_create(&root->db, buf, f_count, f_lens) < 0) {
			free(root->dir);
			return -1;
		}
		database_create_map(&root->db, ARTICLE_URI_FIELD);
		database_create_map(&root->db, ARTICLE_DATE_FIELD);
	}
	return 0;
}


int article_get(article_root *root, article **dest, const char *uri) {
	const char *ptr = uri;
	if ('0' <= *ptr && *ptr <= '9') {
		uint year, month, day;
		uint32_t time0, time1;
		if (to_uint(&ptr, &year ) < 0 || !(ptr++) || // !x if x != 0 --> 0
		    to_uint(&ptr, &month) < 0 || !(ptr++) ||
		    to_uint(&ptr, &day  ) < 0)
			return -1;
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
		return article_get_between_times(root, dest, time0, time1);
	} else if (*ptr == 0) {
		return article_get_between_times(root, dest, 0, 0xFFffFFff);	
	} else {
		article *art = *dest = calloc(1, sizeof(article));

		char dburi[ARTICLE_FILE_LEN];
		memset(dburi, 0, sizeof(dburi));
		strncpy(dburi, uri, sizeof(dburi));
		char entry[root->db.entry_length];
		if (database_get(&root->db, entry, ARTICLE_URI_FIELD, dburi) < 0)
			goto error;
		
		char *ptr = art->file;
		size_t l = strlen(root->dir);
		memcpy(ptr, root->dir, l);
		ptr += l;
		if (database_get_field(&root->db, ptr, entry, ARTICLE_FILE_FIELD) < 0)
			goto error;
		ptr[strlen(ptr)] = 0;
	
		uint32_t date;
		if (database_get_field(&root->db, (char *)&art->date, entry, ARTICLE_DATE_FIELD  ) < 0 ||
		    database_get_field(&root->db,  art->title , entry, ARTICLE_TITLE_FIELD ) < 0 ||
		    database_get_field(&root->db,  art->author, entry, ARTICLE_AUTHOR_FIELD) < 0);
		
		return 1;
	error:
		free(art);
		return -1;
	}
}

int article_get_comments(article_root *root, article_comment **dest, article *art);
