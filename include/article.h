#ifndef ARTICLE_H
#define ARTICLE_H


#include <sys/stat.h>
#include "database.h"


#define DB_URI_LEN      64
#define DB_FILE_LEN     256
#define DB_DATE_LEN     4
#define DB_AUTHOR_LEN   64
#define DB_TITLE_LEN    64

#define DB_URI_FIELD    0
#define DB_FILE_FIELD   1
#define DB_DATE_FIELD   2
#define DB_AUTHOR_FIELD 3
#define DB_TITLE_FIELD  4


typedef unsigned int uint;

/*
 * An article root is a wrapper around a database containing article entries.
 * Each entry contains the URI, the file path, the date of submission, the
 * author's name and the title.
 */
typedef struct article_root {
	struct database db;
	char *dir;
} article_root;


/*
 * Loads or creates a new article database for the given path.
 */
int article_init(article_root *root, const char *path);

/*
 * Frees memory and stores any changes to the database
 */
void article_free(article_root *root);

/*
 * Looks an article up for the given URI and returns it contents if found,
 * otherwise it returns NULL.
 */
const char *article_get(article_root *root, const char *uri);

/*
 * Formats the given year, month, day, hour and minute into a single 32-bit
 * integer. This format is used in the database.
 */
uint32_t format_date(uint year, uint month, uint day, uint hour, uint minute);

/*
 * Converts a date to a string. The default format is YYYY-MM-DD hh:mm
 */
int date_to_str(char *buf, uint32_t date);

#endif
