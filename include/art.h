#ifndef ART_H
#define ART_H


#include <sys/stat.h>
#include <stdint.h>
#include "list.h"


typedef unsigned int uint;

/*
 * An article root is a wrapper around a database containing article entries.
 * Each entry contains the URI, the file path, the date of submission, the
 * author's name and the title.
 */

struct date {
	union {
		struct {
			uint8_t  min;
			uint8_t  hour;
			uint8_t  day;
			uint8_t  month;
			uint32_t year;
		};
		uint64_t num;
	};
};

typedef struct art_root {
	list  articles;
	char *dir;
} *art_root;

typedef struct comment {
	char        *body;
	list         replies;
	int          id;
	int          reply_to;
	struct date  date;
	char        *author;
} *comment;


typedef struct article {
	char           *uri;
	char           *file;
	struct date     date;
	char           *author;
	char           *title;
	struct article *next;
	struct article *prev;
} *article;


/*
 * Loads or creates a new article database for the given path.
 */
art_root art_load(const char *path);

/*
 * Get the comments by an article
 */
list art_get_comments(art_root root, const char *uri);

/*
 *
 */
void art_free_comments(list ls);

int art_add_comment(art_root root, const char *uri, comment c, size_t reply_to);

/*
 * Frees memory and stores any changes to the database
 */
void art_free(art_root root);

/*
 * Looks an article up for the given URI and returns it contents if found,
 * otherwise it returns NULL.
 */
list art_get(art_root root, const char *uri);

/*
 * Parse a date in the following format:
 * - 12 bits for the year
 * -  4 bits for the month
 * -  5 bits for the day
 * - 11 bits for the hour and minute
 */
uint32_t format_date(uint year, uint month, uint day, uint hour, uint minute);

#endif
