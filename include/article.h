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


typedef struct article_root {
	struct database db;
	char *dir;
} article_root;


int article_init(article_root *root, const char *path);

const char *article_get(article_root *root, const char *uri);

uint32_t format_date(uint year, uint month, uint day, uint hour, uint minute);

int date_to_str(char *buf, uint32_t date);

#endif
