#ifndef ART_H
#define ART_H


#include <sys/stat.h>
#include "db.h"
#include "list.h"


#define ART_URI_LEN      64
#define ART_FILE_LEN     256
#define ART_DATE_LEN     4
#define ART_AUTHOR_LEN   64
#define ART_TITLE_LEN    64

#define ART_URI_FIELD    0
#define ART_FILE_FIELD   1
#define ART_DATE_FIELD   2
#define ART_AUTHOR_FIELD 3
#define ART_TITLE_FIELD  4

#define ART_COMM_ID_FIELD     0
#define ART_COMM_AUTHOR_FIELD 1
#define ART_COMM_DATE_FIELD   2
#define ART_COMM_INDEX_FIELD  3
#define ART_COMM_LENGTH_FIELD 4
#define ART_COMM_REPLY_FIELD  5


typedef unsigned int uint;

/*
 * An article root is a wrapper around a database containing article entries.
 * Each entry contains the URI, the file path, the date of submission, the
 * author's name and the title.
 */
typedef struct art_root {
	struct database db;
	char *dir;
} art_root;


typedef struct art_comment {
	uint32_t id;
	char   *body;
	list   replies;
	uint32_t reply_to;
	uint32_t date;
	char   author[ART_AUTHOR_LEN + 1];
} art_comment;


typedef struct article {
	char uri   [ART_URI_LEN    + 1];
	char file  [ART_FILE_LEN*2 + 1];
	uint32_t date;
	char author[ART_AUTHOR_LEN + 1];
	char title [ART_TITLE_LEN  + 1];
	char next  [ART_URI_LEN    + 1];
	char prev  [ART_URI_LEN    + 1];
} article;


/*
 * Loads or creates a new article database for the given path.
 */
int art_init(art_root *root, const char *path);

/*
 * Get one or more articles from a database
 */
int art_get(art_root *root, article **dest, const char *uri);

/*
 * Get the comments by an article
 */
list art_get_comments(art_root *root, const char *uri);

/*
 *
 */
void art_free_comments(list ls);

/*
 * Frees memory and stores any changes to the database
 */
void art_free(art_root *root);

/*
 * Looks an article up for the given URI and returns it contents if found,
 * otherwise it returns NULL.
 */
int art_get(art_root *root, article **dest, const char *uri);

/*
 * Parse a date in the following format:
 * - 12 bits for the year
 * -  4 bits for the month
 * -  5 bits for the day
 * - 11 bits for the hour and minute
 */
uint32_t format_date(uint year, uint month, uint day, uint hour, uint minute);

/*
 * Converts a date to a string. The default format is YYYY-MM-DD hh:mm
 */
int date_to_str(char *buf, uint32_t date);


#endif
