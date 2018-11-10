#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h> // htonl()
#include "../include/article.h"
#include "../include/db.h"


static void help(const char *name, const char *cmd) {
	if (cmd == NULL) {
		fprintf(stderr, "Usage: %s <command> [options]\n", name);
		fprintf(stderr, "Available options:\n");
		fprintf(stderr, "  %s add <file>\n", name);
		fprintf(stderr, "  %s rm  <file>\n", name);
		fprintf(stderr, "  %s list [name|date]\n", name);
	}
}


static int list_entries(database *db, uint8_t field)
{
	const char **entries;
	uint32_t count = db_get_all_entries(db, (void *)&entries, field);
	for (size_t i = 0; i < count; i++) {
		char uri[ARTICLE_URI_LEN+1], file[ARTICLE_FILE_LEN+1], datestr[64],
		     author[ARTICLE_AUTHOR_LEN+1], title[ARTICLE_TITLE_LEN+1];
		uint32_t date;
		memset(uri   , 0, sizeof(uri   ));
		memset(file  , 0, sizeof(file  ));
		memset(author, 0, sizeof(author));
		memset(title , 0, sizeof(title ));
		db_get_field(db, uri   , entries[i], ARTICLE_URI_FIELD   );
		db_get_field(db, file  , entries[i], ARTICLE_FILE_FIELD  );
		db_get_field(db, (char *)&date , entries[i], ARTICLE_DATE_FIELD  );
		db_get_field(db, author, entries[i], ARTICLE_AUTHOR_FIELD);
		db_get_field(db, title , entries[i], ARTICLE_TITLE_FIELD );
		date_to_str(datestr, htonl(date));
		printf("'%s' by %s @ %s (%s --> %s)\n",
		       title, author, datestr, uri, file);
	}
	return 0;
}


int main(int argc, char **argv)
{
	char uri[ARTICLE_URI_LEN+1], file[ARTICLE_FILE_LEN+1],
             author[ARTICLE_AUTHOR_LEN+1], title[ARTICLE_TITLE_LEN+1];
	memset(uri   , 0, sizeof(uri   ));
	memset(file  , 0, sizeof(file  ));
	memset(author, 0, sizeof(author));
	memset(title , 0, sizeof(title ));
	char *dbfile;
	uint32_t date;
	database db;	

	// Necesarry to write stuff without newlines and not having to write fflush constantly
	setbuf(stdout, NULL); 

	if (argc < 2) {
		help(argv[0], NULL);
		return 1;
	}
	dbfile = argv[1];
	
	if (db_load(&db, dbfile) < 0) {
		fprintf(stderr, "Couldn't open '%s': ", dbfile);
		perror(NULL);
		return 1;
	}

	return -list_entries(&db, 0);

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
	date = htonl(date); // Using integers with memcmp requires big-endianness

	char buf[db.entry_length];
	db_set_field(&db, buf, ARTICLE_TITLE_FIELD , title );
	db_set_field(&db, buf, ARTICLE_URI_FIELD   , uri   );
	db_set_field(&db, buf, ARTICLE_FILE_FIELD  , file  );
	db_set_field(&db, buf, ARTICLE_AUTHOR_FIELD, author);
	db_set_field(&db, buf, ARTICLE_DATE_FIELD  , &date );
	db_add(&db, buf);

	if (db_add(&db, buf) < 0) {
		fprintf(stderr, "Failed to add another entry (perhaps you were too soon?)\n");
		return 1;
	}

	date_to_str(buf, htonl(date));
	printf("Entry '%s' added @ '%s'\n", title, buf);

	db_free(&db);
	return 0;
}
