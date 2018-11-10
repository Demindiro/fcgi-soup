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
#include "../include/db.h"
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


static int article_get_between_times(article_root *root, article **dest, uint32_t t0, uint32_t t1)
{
	t0 = htonl(t0);
	t1 = htonl(t1);
	const char **entries;
	size_t count = db_get_range(&root->db, (void *)&entries, ARTICLE_DATE_FIELD, &t0, &t1);
	article *arts = *dest = calloc(count, sizeof(article));
	for (size_t i = 0; i < count; i++) {
		db_get_field(&root->db, arts[i].uri   , entries[i], ARTICLE_URI_FIELD   );
		db_get_field(&root->db, arts[i].file  , entries[i], ARTICLE_FILE_FIELD  );
		db_get_field(&root->db, (char *)&arts[i].date  , entries[i], ARTICLE_DATE_FIELD  );
		db_get_field(&root->db, arts[i].author, entries[i], ARTICLE_AUTHOR_FIELD);
		db_get_field(&root->db, arts[i].title , entries[i], ARTICLE_TITLE_FIELD );
	}
	free(entries);
	return count;
}


static int get_comment(article_comment *dest, const char *entry, database *db, int fd)
{
	uint32_t index, length;
	db_get_field(db, &dest->reply_to, entry, ARTICLE_REPLY_FIELD );
	db_get_field(db, &dest->author  , entry, ARTICLE_AUTHOR_FIELD);
	db_get_field(db, &index         , entry, ARTICLE_INDEX_FIELD );
	db_get_field(db, &length        , entry, ARTICLE_LENGTH_FIELD);
	dest->body = malloc(length + 1);
	if (dest->body == NULL)
		return -1;
	lseek(fd, SEEK_SET, index);
	read(fd, dest->body, length);
	dest->body[length] = 0;
	return 0;
}


int article_get_comments(article_root *root, list *dest, const char *name)
{
	char file[256], *ptr = file;
	size_t l = strlen(root->dir);

	memcpy(ptr, root->dir, l);
	ptr += l;
	l = sizeof("comments");
	memcpy(ptr, "comments/", l);
	ptr += l;
	l = strlen(name);
	memcpy(ptr, name, l);
	ptr += l;

	memcpy(ptr, "comments", sizeof("comments"));
	int fd = open(file, O_RDONLY);
	if (fd == -1)
		goto error;

	memcpy(ptr, "db", sizeof("db"));
	database db;
	if (db_load(&db, name) < 0)
		goto error;
	const char **entries;
	uint32_t count = db_get_all_entries(&db, (void *)&entries, 0);
	if (count == -1)
		goto error;

	for (size_t i = 0; i < count; i++) {
		article_comment comment;	
		if (get_comment(&comment, entries[i], &db, fd) < 0)
			goto error;
		if (list_add(dest, &comment) < 0)
			goto error;
	}

	int r = 0;
	goto success;
error:
	r = -1;
success:
	db_free(&db);
	close(fd);
	return r;
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
	if (db_load(&root->db, buf) < 0) {
		uint8_t f_count = 5;
		uint16_t f_lens[5] = {
			[ARTICLE_URI_FIELD   ] = ARTICLE_URI_LEN   ,
			[ARTICLE_FILE_FIELD  ] = ARTICLE_FILE_LEN  ,
			[ARTICLE_DATE_FIELD  ] = ARTICLE_DATE_LEN  ,
			[ARTICLE_AUTHOR_FIELD] = ARTICLE_AUTHOR_LEN,
			[ARTICLE_TITLE_FIELD ] = ARTICLE_TITLE_LEN ,
		}; 
		if (db_create(&root->db, buf, f_count, f_lens) < 0) {
			free(root->dir);
			return -1;
		}
		db_create_map(&root->db, ARTICLE_URI_FIELD);
		db_create_map(&root->db, ARTICLE_DATE_FIELD);
	}
	return 0;
}


int article_get(article_root *root, article **dest, const char *uri) {
	const char *ptr = uri;
	if ('0' <= *ptr && *ptr <= '9') {
		uint year = 0, month = 0, day = 0;
		uint32_t time0, time1;
		if (to_uint(&ptr, &year ) < 0)
			return -1;
                if (*ptr != 0 && ptr++ && to_uint(&ptr, &month) < 0)
			return -1;
		if (*ptr != 0 && ptr++ && to_uint(&ptr, &day  ) < 0)
			return -1;
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
		return article_get_between_times(root, dest, time0, time1);
	} else if (*ptr == 0) {
		return article_get_between_times(root, dest, 0, 0xFFffFFff);	
	} else {
		article *art = *dest = calloc(1, sizeof(article));

		char dburi[ARTICLE_FILE_LEN];
		memset(dburi, 0, sizeof(dburi));
		strncpy(dburi, uri, sizeof(dburi));
		const char *entry = db_get(&root->db, ARTICLE_URI_FIELD, dburi);
		if (entry == NULL)
			goto error;
		
		char *ptr = art->file;
		size_t l = strlen(root->dir);
		memcpy(ptr, root->dir, l);
		ptr += l;
		if (db_get_field(&root->db, ptr, entry, ARTICLE_FILE_FIELD) < 0)
			goto error;
		ptr[strlen(ptr)] = 0;
	
		if (db_get_field(&root->db, (char *)&art->date, entry, ARTICLE_DATE_FIELD  ) < 0 ||
		    db_get_field(&root->db,  art->title , entry, ARTICLE_TITLE_FIELD ) < 0 ||
		    db_get_field(&root->db,  art->author, entry, ARTICLE_AUTHOR_FIELD) < 0)
			/* TODO */;

		const char *prev = db_get_offset(&root->db, ARTICLE_URI_FIELD, dburi, -1),
		           *next = db_get_offset(&root->db, ARTICLE_URI_FIELD, dburi,  1);
		if (prev != NULL)
			memcpy(art->prev, prev, ARTICLE_URI_LEN);
		if (next != NULL)
			memcpy(art->next, next, ARTICLE_URI_LEN);

		strncpy(art->uri, uri, ARTICLE_URI_LEN);

		return 1;
	error:
		free(art);
		return -1;
	}
}


void article_free(article_root *root)
{
	db_free(&root->db);
	free(root->dir);
}
