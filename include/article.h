#ifndef ARTICLE_H
#define ARTICLE_H

#include <sys/stat.h>


typedef struct article_root {
	char *root;
	int count;
	int size;
	char **names;
	struct stat *stats;
} article_root;


int article_init(article_root *root, const char *path);

int article_get(article_root *root, const char *uri);


#endif
