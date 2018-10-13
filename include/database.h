#ifndef DATABASE_H
#define DATABASE_H

#include <stdint.h>


typedef struct database {
	char    *data; 
	uint32_t count;
	uint32_t entry_length;
	uint8_t  field_count;
	uint16_t field_lengths[16];
	void    *mapptr;
} database;


int database_load(database *db, const char *file);

void database_free(database *db);

int database_get(database *db, char *buf, uint8_t field, const char *key);

int database_set(database *db, uint8_t keyfield, const char *key, uint8_t valfield, const char *val);

int database_add(database *db, const char *entry);

int database_del(database *db, uint8_t keyfield, const char *key);


#endif
