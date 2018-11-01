#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../include/article.h"
#include "../include/database.h"

int main(int argc, char **argv)
{
	char uri[ARTICLE_URI_LEN+1], file[ARTICLE_FILE_LEN+1], author[ARTICLE_AUTHOR_LEN+1], title[ARTICLE_TITLE_LEN+1];
	memset(uri   , 0, sizeof(uri   ));
	memset(file  , 0, sizeof(file  ));
	memset(author, 0, sizeof(author));
	memset(title , 0, sizeof(title ));
	char *dbfile;
	uint32_t date;
	database db;	

	// Necesarry to write stuff without newlines and not having to write fflush constantly
	setbuf(stdout, NULL); 

	if (argc != 2) {
		printf("Usage: %s <file>\n", argv[0]);
		return 1;
	}
	dbfile = argv[1];
	
	if (database_load(&db, dbfile) < 0) {
		fprintf(stderr, "Couldn't open '%s': ", dbfile);
		perror(NULL);
		return 1;
	}

	printf("Title: ");
	fgets(title , sizeof(title ), stdin);
	printf("URI: ");
	fgets(uri   , sizeof(uri   ), stdin);
	printf("File: ");
	fgets(file  , sizeof(file  ), stdin);
	printf("Author: ");
	fgets(author, sizeof(author), stdin);

	title [strlen(title )-1] = 0;
	uri   [strlen(uri   )-1] = 0;
	file  [strlen(file  )-1] = 0;
	author[strlen(author)-1] = 0;

	time_t tt = time(NULL);
	struct tm *t = localtime(&tt);
	date = format_date(t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min);

	char buf[db.entry_length];
	database_set_field(&db, buf, ARTICLE_TITLE_FIELD , title );
	database_set_field(&db, buf, ARTICLE_URI_FIELD   , uri   );
	database_set_field(&db, buf, ARTICLE_FILE_FIELD  , file  );
	database_set_field(&db, buf, ARTICLE_AUTHOR_FIELD, author);
	database_set_field(&db, buf, ARTICLE_DATE_FIELD  , &date );
	database_add(&db, buf);

	date_to_str(buf, date);
	printf("Entry '%s' added @ '%s'\n", title, buf);

	database_free(&db);
	return 0;
}
