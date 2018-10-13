#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/database.h"


// 1U << 30 == 1 Gigabyte
// This should be sufficient for most usecases (larger databases are probably best
// stored inside multiple files)
#define MMAP_SIZE (1U << 30)


static size_t get_offset(database *db, uint8_t field)
{
	size_t offset = 0;
	for (size_t i = 0; i < field; i++)
		offset += db->field_lengths[i];
	return offset;
}


static char *get_entry_ptr(database *db, uint8_t keyfield, const char *key)
{
	size_t f_offset = get_offset(db, keyfield);
	for (size_t i = 0; i < db->count; i++) {
		char *entry = db->data + (i * db->entry_length);
		if (memcmp(entry + f_offset, key, db->field_lengths[i]) == 0)
			return entry;
	}
	return NULL;
}


int database_create(database *db, const char *file, uint8_t field_count, uint16_t *field_lengths)
{
	int fd = open(file, O_RDWR);
	if (fd < 0)
		return -1;
	char *map = db->mapptr = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (db->mapptr == NULL)
		return -1;
	*((uint32_t *)map) = db->count = 0;
	map += sizeof(db->count);
	*((uint8_t  *)map) = db->field_count  = field_count;
	map += sizeof(db->field_count);
	db->entry_length = 0;
	for (size_t i = 0; i < field_count; i++) {
		*((uint16_t *)map) = db->field_lengths[i] = field_lengths[i];
		map += sizeof(*db->field_lengths);
		db->entry_length  += field_lengths[i];
	}
	db->data = map;
	return 0;
}


int database_load(database *db, const char *file)
{
	int fd = open(file, O_RDWR);
	if (fd < 0)
		return -1;
	char *map = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == NULL)
		return -1;
	db->mapptr = map;
	db->count = *((uint32_t *)map);
	map += sizeof(db->count);
	db->field_count = *((uint8_t *)map);
	map += sizeof(db->field_count);
	db->entry_length = 0;
	for (int i = 0; i < db->field_count; i++) {
		db->field_lengths[i] = *((uint16_t *)map);
		db->entry_length    += db->field_lengths[i];
		map += sizeof(*db->field_lengths);
	}
	db->data = map;
	return 0;
}


void database_free(database *db)
{
	munmap(db->mapptr, MMAP_SIZE);
}


int database_get(database *db, char *buf, uint8_t keyfield, const char *key)
{
	const char *entry = get_entry_ptr(db, keyfield, key);
	if (entry == NULL)
		return -1;
	memcpy(buf, entry, db->entry_length);
	return 0;
}


int database_set(database *db, uint8_t keyfield, const char *key, uint8_t valfield, const char *val)
{
	char *entry = get_entry_ptr(db, keyfield, key);
	if (entry == NULL)
		return -1;
	size_t f_offset = get_offset(db, valfield);
	memcpy(entry + f_offset, val, db->field_lengths[valfield]);
	return 0;
}


int database_add(database *db, const char *entry)
{
	memcpy(db->data + (db->count * db->entry_length), entry, db->entry_length);
	db->count++;
	return 0;
}


int database_del(database *db, uint8_t keyfield, const char *key)
{
	char *entry = get_entry_ptr(db, keyfield, key);
	if (entry == NULL)
		return -1;
	memset(entry, 0, db->entry_length);
	return 0;
}


int database_get_field(database *db, char *buf, const char *entry, uint8_t field)
{
	size_t f_offset = get_offset(db, field);
	memcpy(buf, entry + f_offset, db->field_lengths[field]);
	return 0;
}


int database_get_range(database *db, const char ***entries, uint8_t field, const void *key0, const void *key1)
{
	size_t i = 0, start, end;
	size_t f_offset = get_offset(db, field);
	for ( ; i < db->count; i++) {
		char *entry = db->data + (i * db->entry_length);
		if (memcmp(key0, entry + f_offset, db->entry_length) <= 0) {
			start = i;
			goto found_start;
		}
	}
	*entries = NULL;
	return 0;
found_start:
	for ( ; i < db->count; i++) {
		char *entry = db->data + (i * db->entry_length);
		if (memcmp(entry + f_offset, key1, db->entry_length) < 0) {
			end = i;
			goto found_end;
		}
	}
	end = db->count;
found_end:
	*entries = malloc((end - start) * sizeof(char *));
	for (size_t j = 0, i = start; i < end; i++)
		(*entries)[j] = db->data + (i * db->entry_length);
	return 0;	
}
