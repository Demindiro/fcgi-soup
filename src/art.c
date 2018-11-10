/*
 * Database format:
 *   -  64 bytes for the uri
 *   - 256 bytes for the name
 *   -   4 bytes for the date
 *     - 12 bits for the year        (covers 4096 years)
 *     -  4 bits for the month       (covers 12 months)
 *     -  5 bits for the day         (covers 31 days)
 *     - 11 bits for the time of day (covers 1440 minutes)
 *   -  64 bytes for the author's name
 *   -  64 bytes for the title
 */

#include "../include/art.h"
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
#include "../include/dict.h"
#include "../include/temp.h"


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


static int art_get_between_times(art_root *root, article **dest, uint32_t t0, uint32_t t1)
{
	t0 = htonl(t0);
	t1 = htonl(t1);
	const char **entries;
	size_t count = db_get_range(&root->db, (void *)&entries, ART_DATE_FIELD, &t0, &t1);
	article *arts = *dest = calloc(count, sizeof(article));
	for (size_t i = 0; i < count; i++) {
		db_get_field(&root->db, arts[i].uri   , entries[i], ART_URI_FIELD   );
		db_get_field(&root->db, arts[i].file  , entries[i], ART_FILE_FIELD  );
		db_get_field(&root->db, (char *)&arts[i].date  , entries[i], ART_DATE_FIELD  );
		db_get_field(&root->db, arts[i].author, entries[i], ART_AUTHOR_FIELD);
		db_get_field(&root->db, arts[i].title , entries[i], ART_TITLE_FIELD );
	}
	free(entries);
	return count;
}


static int get_comment(art_comment *dest, const char *entry, database *db, int fd)
{
	uint32_t index, length;
	db_get_field(db, &dest->reply_to, entry, ART_COMM_REPLY_FIELD );
	db_get_field(db, &dest->author  , entry, ART_COMM_AUTHOR_FIELD);
	db_get_field(db, &dest->id      , entry, ART_COMM_ID_FIELD    );
	db_get_field(db, &dest->date    , entry, ART_COMM_DATE_FIELD  );
	db_get_field(db, &index         , entry, ART_COMM_INDEX_FIELD );
	db_get_field(db, &length        , entry, ART_COMM_LENGTH_FIELD);
	dest->body = malloc(length + 1);
	if (dest->body == NULL)
		return -1;
	lseek(fd, index, SEEK_SET);
	read(fd, dest->body, length);
	dest->body[length] = 0;
	dest->replies = list_create(sizeof(art_comment));
	return 0;
}


list art_get_comments(art_root *root, const char *name)
{
	char file[256], *ptr = file;
	size_t l = strlen(root->dir);
	const char **entries = NULL;
	art_comment *comments = NULL;
	list ls = NULL;
 
	memcpy(ptr, root->dir, l);
	ptr += l;
	l = sizeof("comments");
	memcpy(ptr, "comments/", l);
	ptr += l;
	l = strlen(name);
	memcpy(ptr, name, l);
	ptr += l;
	*ptr = 0;

	int fd = open(file, O_RDONLY);
	if (fd == -1)
		goto error;

	memcpy(ptr, ".db", sizeof(".db"));
	database db;
	if (db_load(&db, file) < 0)
		goto error;
	uint32_t count = db_get_all_entries(&db, (void *)&entries, 0);
	if (count == -1)
		goto error;

	comments = malloc(count * sizeof(art_comment));
	for (size_t i = 0; i < count; i++) {
		if (get_comment(&comments[count - i - 1], entries[i], &db, fd) < 0)
			goto error;
	}

	ls = list_create(sizeof(art_comment));
	for (size_t i = 0; i < count; i++) {
		art_comment *c = &comments[i];
		list l = c->reply_to == -1 ? ls : comments[c->reply_to].replies;
		if (list_add(l, c) < 0)
			goto error;
	}

	goto success;
error:
	free(ls);
	ls = NULL;
success:
	free(entries);
	free(comments);
	db_free(&db);
	close(fd);
	return ls;
}


void art_free_comments(list ls)
{
	for (size_t i = 0; i < ls->count; i++) {
		art_comment *c = (art_comment *)list_get(ls, i);
		free(c->body);
		art_free_comments(c->replies);
	}
	list_free(ls);
}


int art_init(art_root *root, const char *path)
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
			[ART_URI_FIELD   ] = ART_URI_LEN   ,
			[ART_FILE_FIELD  ] = ART_FILE_LEN  ,
			[ART_DATE_FIELD  ] = ART_DATE_LEN  ,
			[ART_AUTHOR_FIELD] = ART_AUTHOR_LEN,
			[ART_TITLE_FIELD ] = ART_TITLE_LEN ,
		}; 
		if (db_create(&root->db, buf, f_count, f_lens) < 0) {
			free(root->dir);
			return -1;
		}
		db_create_map(&root->db, ART_URI_FIELD);
		db_create_map(&root->db, ART_DATE_FIELD);
	}
	return 0;
}


int art_get(art_root *root, article **dest, const char *uri) {
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
		return art_get_between_times(root, dest, time0, time1);
	} else if (*ptr == 0) {
		return art_get_between_times(root, dest, 0, 0xFFffFFff);	
	} else {
		article *art = *dest = calloc(1, sizeof(article));

		char dburi[ART_FILE_LEN];
		memset(dburi, 0, sizeof(dburi));
		strncpy(dburi, uri, sizeof(dburi));
		const char *entry = db_get(&root->db, ART_URI_FIELD, dburi);
		if (entry == NULL)
			goto error;
		
		char *ptr = art->file;
		size_t l = strlen(root->dir);
		memcpy(ptr, root->dir, l);
		ptr += l;
		if (db_get_field(&root->db, ptr, entry, ART_FILE_FIELD) < 0)
			goto error;
		ptr[strlen(ptr)] = 0;

		if (db_get_field(&root->db, (char *)&art->date, entry, ART_DATE_FIELD  ) < 0 ||
		    db_get_field(&root->db,  art->title , entry, ART_TITLE_FIELD ) < 0 ||
		    db_get_field(&root->db,  art->author, entry, ART_AUTHOR_FIELD) < 0)
			/* TODO */;

		const char *prev = db_get_offset(&root->db, ART_URI_FIELD, dburi, -1),
		           *next = db_get_offset(&root->db, ART_URI_FIELD, dburi,  1);
		if (prev != NULL)
			memcpy(art->prev, prev, ART_URI_LEN);
		if (next != NULL)
			memcpy(art->next, next, ART_URI_LEN);

		strncpy(art->uri, uri, ART_URI_LEN);

		return 1;
	error:
		free(art);
		return -1;
	}
}


void art_free(art_root *root)
{
	db_free(&root->db);
	free(root->dir);
}
