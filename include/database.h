#ifndef DATABASE_H
#define DATABASE_H


#include <stdint.h>
#include <sys/types.h>


#define DB_MAX_FIELDS 16

/*
 * A map is a sorted dictionary of keys mapping to indices. A map allows fast
 * lookup of entries: O(log n) instead of O(n) for a single entry.
 * A map is stored in a separate file whose name is identical to the main
 * database's name but with "mapX" appended, where X is a number or letter
 * ranging from 0 to Z depending on which field it uses as key.
 */
typedef struct database_map {
	char     *data;
	union { // The first four bytes happen to be the count, so it points to the same address as mapptr
		uint32_t *count;
		void     *mapptr;
	};
} database_map;

/*
 * A database contains a list of entries with any number and type of fields.
 * Each field can have a different length.
 * A database may also have mappings of keys to indices to reduce lookup time.O
 */
typedef struct database {
	char         *data; 
	union {
		uint32_t     *count;
		void         *mapptr;
	};
	uint32_t      entry_length;
	uint8_t       field_count;
	uint16_t      field_lengths[DB_MAX_FIELDS];
	database_map  maps[DB_MAX_FIELDS];
	char          name[256];
	/* NFS_PATCH */
	time_t        mtime;
	/* === */
} database;

/*
 * Creates a new database linked to a file. A new database has no mappings.
 */
int database_create(database *db, const char *file, uint8_t field_count, uint16_t *field_lengths);

/*
 * Loads an existing database. Existing mappings are loaded with the database.
 */
int database_load(database *db, const char *file);

/*
 * Unloads an existing database and its mappings.
 */
void database_free(database *db);

/*
 * Creates a new mapping with the specified field as key.
 */
int database_create_map(database *db, uint8_t field);

/*
 * Searches for an entry and returns it. If the entry is not found, NULL is
 * returned.
 */
const char *database_get(database *db, uint8_t field, const char *key);

/*
 * Searches for an entry and returns the entry located `offset` entries after
 * the other found eentry. If the entry is not found or no valid entry exists
 * `offset` entries after the found entry, NULL is returned. This methods only
 * works for fields which have a mapping. If no ammping exists, NULL is
 * returned.
 */
const char *database_get_offset(database *db, uint8_t field, const char *key, ssize_t offset);

/*
 * Adds an entry to the database.
 * On success, 0 is returned. If the entry could not be added, -1 is returned.
 */
int database_add(database *db, const char *entry);

/*
 * Removes an entry from the database.
 * On success, 0 is returned. If the field is out of range or the entry is not
 * found, -1 is returned.
 */
int database_del(database *db, uint8_t keyfield, const char *key);

/*
 * Copies the value of a field of a given entry to the given buffer.
 * On success, 0 is returned, If the field is out of range, -1 is returned.
 */
int database_get_field(database *db, char *buf, const char *entry, uint8_t field);

/*
 * Sets the field of the given entry to the given value.
 * On success, 0 is returned. If the field is out of range, -1 is returned.
 */
int database_set_field(database *db, char *entry, uint8_t field, const void *val);

/*
 * Sets the field of the entry specified by the key. If a mapping is associated
 * with the field, the mapping is also updated.
 * On success, 0 is returned. If the entry has not been found, -1 is returned.
 */
int database_set_entry_field(database *db, uint8_t keyfield, const char *key,
                                           uint8_t valfield, const char *value);

/*
 * Returns all entries whose fields are within the values of the given keys.
 * Fields are compared as if they are integers.
 */
uint32_t database_get_range(database *db, const char ***entries, uint8_t field,
                                          const void *key1, const void *key2);


#endif
