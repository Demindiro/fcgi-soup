#include <errno.h>
/* NFS_PATCH */
#include <stdio.h> // perror()
/* === */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "util/string.h"
#include "../include/database.h"


// 1U << 30 == 1 Gigabyte
// This should be sufficient for most usecases (larger databases are probably best
// stored inside multiple files)
#define MMAP_SIZE (1U << 30)


/* NFS_PATCH */
#undef MAP_SHARED
#define MAP_SHARED MAP_PRIVATE
/* === */


static size_t get_offset(database *db, uint8_t field)
{
	size_t offset = 0;
	for (size_t i = 0; i < field; i++)
		offset += db->field_lengths[i];
	return offset;
}


static int get_map_name(database *db, uint8_t field, char *buf, size_t size)
{
	size_t strl = strlen(db->name);
	if (strl > size - 5)
		return -1;
	memcpy(buf, db->name, strl);
	buf[strl++] = 'm';
	buf[strl++] = 'a';
	buf[strl++] = 'p';
	buf[strl++] = field < 10 ? field + '0' : (field - 10) + 'A';
	buf[strl  ] =  0;
	return 0;
}


/* NFS_PATCH */
static int sync_db(database *db)
{
	struct stat s;
	if (stat(db->name, &s) < 0)
		return -1;
	if (s.st_mtime != db->mtime) {
		database db2;
		if (database_load(&db2, db->name) < 0)
			return -1;
		for (size_t i = 0; i < db->field_count; i++)
			if (db->maps[i].mapptr != NULL)
				munmap(db->maps[i].mapptr, MMAP_SIZE);
		munmap(db->mapptr, MMAP_SIZE);
		*db = db2;
	}
	return 0;
}
/* === */


int database_create(database *db, const char *file, uint8_t field_count, uint16_t *field_lengths)
{
	int fd = open(file, O_RDWR | O_CREAT, 0644);
	if (fd < 0)
		return -1;
	// TODO this should grow dynamicallyy
	if (ftruncate(fd, 4096 << 3) < 0) {
		close(fd);
		return -1;
	}
	char *map = db->mapptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	/* NFS_PATCH */
	struct stat s;
	if (fstat(fd, &s) < 0)
		return -1;
	db->mtime = s.st_mtime;
	/* === */

	close(fd);
	if (db->mapptr == NULL)
		return -1;
	
	db->count = (uint32_t *)map;
	*db->count = 0;
	map += sizeof(*db->count);
	
	*((uint8_t  *)map) = db->field_count  = field_count;
	map += sizeof(db->field_count);
	db->entry_length = 0;
	for (size_t i = 0; i < field_count; i++) {
		*((uint16_t *)map) = db->field_lengths[i] = field_lengths[i];
		map += sizeof(*db->field_lengths);
		db->entry_length  += field_lengths[i];
	}
	
	strcpy(db->name, file);

	db->data = map;
	
	return 0;
}


int database_load(database *db, const char *file)
{
	int fd = open(file, O_RDWR);
	if (fd < 0)
		return -1;
	char *map = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	/* NFS_PATCH */
	struct stat s;
	if (fstat(fd, &s) < 0)
		return -1;
	db->mtime = s.st_mtime;
	/* === */

	close(fd);
	if (map == NULL)
		return -1;

	db->mapptr = map;

	db->count = (uint32_t *)map;
	map += sizeof(*db->count);

	db->field_count = *((uint8_t *)map);
	map += sizeof(db->field_count);
	db->entry_length = 0;
	for (size_t i = 0; i < db->field_count; i++) {
		db->field_lengths[i] = *((uint16_t *)map);
		db->entry_length    += db->field_lengths[i];
		map += sizeof(*db->field_lengths);
	}

	strcpy(db->name, file);

	for (size_t i = 0; i < db->field_count; i++) {
		char name[256];
		if (get_map_name(db, i, name, sizeof(name)) < 0)
			continue; // TODO
		int mfd = open(name, O_RDWR);
		if (mfd < 0) {
			db->maps[i].data = NULL;
		} else {
			char *kmap = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
			if (map == NULL) {
				/* TODO */
				continue;
			}
			db->maps[i].count  = (uint32_t *)kmap;
			db->maps[i].data   = kmap + sizeof(*db->maps[i].count);
		}
	}
	db->data = map;
	return 0;
}


void database_free(database *db)
{
	/* NFS_PATCH */
	struct stat s;
	int fd = -1;
	if (stat(db->name, &s) < 0)
		goto error;
	if (s.st_mtime != db->mtime)
		goto error;
	fd = open(db->name, O_WRONLY);
	if (fd < 0)
		goto error;
	size_t len = sizeof(*db->count);
	len += sizeof(db->field_count);
	len += sizeof(*db->field_lengths) * db->field_count;
	len += db->entry_length * *db->count;
	if (write(fd, db->count, len) < 0)
		goto error;
	for (size_t i = 0; i < db->field_count; i++) {
		database_map map = db->maps[i];
		if (map.mapptr != NULL) {
			char buf[256];
			if (get_map_name(db, i, buf, sizeof(buf)) < 0)
				goto error;
			int fd = open(buf, O_WRONLY);
			if (fd < 0) {
				close(fd);
				goto error;
			}
			size_t len = sizeof(*map.count);
			len += (*map.count * (db->field_lengths[i] + sizeof(*db->count)));
			write(fd, map.mapptr, len);
			close(fd);
		}
	}
	goto success;
error:
	perror(NULL);
success:
	close(fd);
	/* === */
	for (size_t i = 0; i < db->field_count; i++)
		if (db->maps[i].mapptr != NULL)
			munmap(db->maps[i].mapptr, MMAP_SIZE);
	munmap(db->mapptr, MMAP_SIZE);
}



int database_create_map(database *db, uint8_t field)
{
	char name[256];
	if (field >= db->field_count ||
	    db->maps[field].data != NULL ||
	    get_map_name(db, field, name, sizeof(name)) < 0)
		return -1;
	int fd = open(name, O_RDWR | O_CREAT, 0644);
	if (fd < 0)
		return -1;
	// TODO
	if (ftruncate(fd, 4096 << 3) < 0) {
		close(fd);
		return -1;
	}
	char *map = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (map == NULL)
		return -1;
	db->maps[field].count = (uint32_t *)map;
	db->maps[field].data  = map + sizeof(*db->maps[field].count);
	return 0;	
}


const char *database_get(database *db, uint8_t keyfield, const char *key)
{
	size_t f_offset = get_offset(db, keyfield);
	for (size_t i = 0; i < *db->count; i++) {
		char *entry = db->data + (i * db->entry_length);
		if (memcmp(entry + f_offset, key, db->field_lengths[keyfield]) == 0)
			return entry;
	}
	errno = ENOENT;
	return NULL;
}


const char *database_get_offset(database *db, uint8_t keyfield, const char *key, ssize_t offset)
{
	const char *entry = database_get(db, keyfield, key);
	if (entry == NULL)
		return NULL;
	entry += (ssize_t)db->entry_length * offset;
	if (entry < db->data || entry > db->data + ((db->entry_length - 1) * *db->count))
		return NULL;
	return entry;
}


int database_add(database *db, const char *entry)
{
	size_t indices[db->field_count];
	// Verify first that there are no duplicate fields in the mappings
	for (size_t i = 0; i < db->field_count; i++) {
		database_map map = db->maps[i];
		if (map.data == NULL)
			continue;
		size_t flen = db->field_lengths[i], elen = flen + sizeof(*db->count);
		const char *field = entry + get_offset(db, i);
		for (size_t j = 0; j < *map.count; j++) {
			char *key = map.data + (j * elen);
			int cmp = memcmp(field, key, flen);
			if (cmp == 0)
				return -1;
			if (cmp < 0) {
				indices[i] = j;
				goto next_field;
			}
		}
		indices[i] = *map.count;
		next_field:;
	}
	for (size_t i = 0; i < db->field_count; i++) {
		database_map map = db->maps[i];
		if (map.data == NULL)
			continue;
		const char *field = entry + get_offset(db, i);
		size_t flen = db->field_lengths[i], elen = flen + sizeof(*db->count);
		size_t j = indices[i];
		memmove(map.data + ((j + 1) * elen), map.data + (j * elen),
		        elen * (*map.count - j));
		memcpy(map.data + (j * elen), field, flen);
		*((uint32_t *)(map.data + (j * elen) + flen)) = *db->count;
		(*map.count)++;
	}
	memcpy(db->data + (*db->count * db->entry_length), entry, db->entry_length);
	(*db->count)++;
	msync(db->mapptr, MMAP_SIZE, MS_ASYNC);
	return 0;
}


int database_del(database *db, uint8_t keyfield, const char *key)
{
	char *entry = (char *)database_get(db, keyfield, key);
	if (entry == NULL)
		return -1;
	memset(entry, 0, db->entry_length);
	return 0;
}


// TODO check if a map has to be updated
int database_get_field(database *db, char *buf, const char *entry, uint8_t field)
{
	size_t f_offset = get_offset(db, field);
	memcpy(buf, entry + f_offset, db->field_lengths[field]);
	return 0;
}


int database_set_field(database *db, char *entry, uint8_t field, const void *val)
{
	size_t f_offset = get_offset(db, field);
	memcpy(entry + f_offset, val, db->field_lengths[field]);
	return 0;
}
// === //

uint32_t database_get_range(database *db, const char ***entries, uint8_t field, const void *key0, const void *key1)
{
	sync_db(db);
	if (field >= db->field_count)
		return -1;

	database_map map = db->maps[field];
	if (map.data != NULL) {
		size_t i = 0, start, end;
		size_t flen = db->field_lengths[field], elen = flen + sizeof(*db->count);
		for ( ; i < *map.count; i++) {
			char *entry = map.data + (i * elen);
			if (memcmp(key0, entry, flen) <= 0) {
				start = i;
				goto found_start;
			}
		}
		*entries = NULL;
		return 0;
	found_start:
		for ( ; i < *map.count; i++) {
			char *entry = map.data + (i * elen);
			if (memcmp(key1, entry, flen) < 0) {
				end = i - 1;
				goto found_end;
			}
		}
		end = *map.count;
	found_end:
		if (i == 0)
			return 0;
		size_t count = end - start;
		*entries = malloc(count * sizeof(char *));
		if (*entries == NULL)
			return -1;
		for (size_t j = 0, i = start; j < count; j++, i++) {
			size_t index = *(uint32_t *)(map.data + (i * elen) + flen);
			(*entries)[j] = db->data + (index * db->entry_length);
		}
		return count;
	} else {
		size_t size = 16, len = 0, count = 0;
		size_t f_offset = get_offset(db, field);
		char *buf = malloc(size);
		for (size_t i = 0; i < *db->count; i++) {
			char *entry = db->data + (i * db->entry_length);
			if (memcmp(key0, entry + f_offset, db->entry_length) <= 0 &&
			    memcmp(key1, entry + f_offset, db->entry_length) >= 0) {
				if (buf_write(&buf, &len, &size, entry, sizeof(entry))) {
					free(buf);
					return -1;
				}
				count++;
			}
		}
		*entries = realloc(buf, len);
		return count;
	}
}
