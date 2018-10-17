#ifndef DATABASE_H
#define DATABASE_H

#include <stdint.h>


#define DB_MAX_FIELDS 16


typedef struct database_map {
	char     *data;
	uint32_t *count;
} database_map;

typedef struct database {
	char         *data; 
	uint32_t     *count;
	uint32_t      entry_length;
	uint8_t       field_count;
	uint16_t      field_lengths[DB_MAX_FIELDS];
	database_map  maps[DB_MAX_FIELDS];
	char          name[256];
	void         *mapptr;
} database;


int database_create(database *db, const char *file, uint8_t field_count, uint16_t *field_lengths);

int database_load(database *db, const char *file);

void database_free(database *db);

int database_create_map(database *db, uint8_t field);

int database_get(database *db, char *buf, uint8_t field, const char *key);

int database_add(database *db, const char *entry);

int database_del(database *db, uint8_t keyfield, const char *key);

int database_get_field(database *db, char *buf, const char *entry, uint8_t field);

int database_set_field(database *db, char *entry, uint8_t field, const void *val);

uint32_t database_get_range(database *db, const char ***entries, uint8_t field, const void *key1, const void *key2);


#endif
