#ifndef ARTICLE_H
#define ARTICLE_H

#include <sys/stat.h>


typedef struct article {
	char *name;
	time_t date;
	char *contents;
} article;


typedef struct article_root {
	char *root;
	int count;
	int size;
	article *articles;
} article_root;


int article_init(article_root *root, const char *path);

const char *article_get(article_root *root, const char *uri);


#endif
