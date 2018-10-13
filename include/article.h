#ifndef ARTICLE_H
#define ARTICLE_H


#include <sys/stat.h>
#include "database.h"


typedef struct article_root {
	struct database db;
	char *dir;
} article_root;


int article_init(article_root *root, const char *path);

const char *article_get(article_root *root, const char *uri);


#endif
