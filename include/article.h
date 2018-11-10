#ifndef ARTICLE_H
#define ARTICLE_H


#include <sys/stat.h>
#include "db.h"
#include "container/list.h"


#define ARTICLE_URI_LEN      64
#define ARTICLE_FILE_LEN     256
#define ARTICLE_DATE_LEN     4
#define ARTICLE_AUTHOR_LEN   64
#define ARTICLE_TITLE_LEN    64

#define ARTICLE_URI_FIELD    0
#define ARTICLE_FILE_FIELD   1
#define ARTICLE_DATE_FIELD   2
#define ARTICLE_AUTHOR_FIELD 3
#define ARTICLE_TITLE_FIELD  4

#define ARTICLE_ID_FIELD     0
#define ARTICLE_REPLY_FIELD  1
// Already defined
// ALready defined
#define ARTICLE_INDEX_FIELD  4
#define ARTICLE_LENGTH_FIELD 5

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


typedef struct article_comment {
	char   *body;
	list   replies;
	uint32_t reply_to;
	char   author[ARTICLE_AUTHOR_LEN + 1];
} article_comment;


typedef struct article {
	char uri   [ARTICLE_URI_LEN    + 1];
	char file  [ARTICLE_FILE_LEN*2 + 1];
	uint32_t date;
	char author[ARTICLE_AUTHOR_LEN + 1];
	char title [ARTICLE_TITLE_LEN  + 1];
	char next  [ARTICLE_URI_LEN    + 1];
	char prev  [ARTICLE_URI_LEN    + 1];
} article;


/*
 * Loads or creates a new article database for the given path.
 */
int article_init(article_root *root, const char *path);

/*
 * Get one or more articles from a database
 */
int article_get(article_root *root, article **dest, const char *uri);

/*
 * Get the comments by an article
 */
int article_get_comments(article_root *root, list *dest, const char *uri);

/*
 * Frees memory and stores any changes to the database
 */
void article_free(article_root *root);

/*
 * Looks an article up for the given URI and returns it contents if found,
 * otherwise it returns NULL.
 */
int article_get(article_root *root, article **dest, const char *uri);

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
