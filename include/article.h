#ifndef ARTICLE_H
#define ARTICLE_H


#include <sys/stat.h>
#include "database.h"


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


typedef unsigned int uint;


typedef struct article_root {
	struct database db;
	struct database comments;
	char *dir;
} article_root;


typedef struct article_comment {
	uint32_t id;
	uint32_t reply_to;
	char *body;
	char author[ARTICLE_AUTHOR_LEN + 1];
} article_comment;


typedef struct article {
	char uri   [ARTICLE_URI_LEN    + 1];
	char file  [ARTICLE_FILE_LEN*2 + 1];
	uint32_t date;
	char author[ARTICLE_AUTHOR_LEN + 1];
	char title [ARTICLE_TITLE_LEN  + 1];
} article;


/*
 * Load an article database
 */
int article_init(article_root *root, const char *path);

/*
 * Get one or more articles from a database
 */
int article_get(article_root *root, article **dest, const char *uri);

/*
 * Get the comments by an article
 */
int article_get_comments(article_root *root, article_comment **dest, article *art);

/*
 * Parse a date in the following format:
 * - 12 bits for the year
 * -  4 bits for the month
 * -  5 bits for the day
 * - 11 bits for the hour and minute
 */
uint32_t format_date(uint year, uint month, uint day, uint hour, uint minute);

/*
 * Convert a date to a human-friendly format
 */
int date_to_str(char *buf, uint32_t date);


#endif
