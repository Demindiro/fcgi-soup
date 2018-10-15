soup_src  = src/*.c
dbman_src = dbman/main.c src/article.c src/dictionary.c src/template.c src/database.c

db  = gdb
mem = valgrind


debug:
	gcc $(soup_src) -lfcgi -Og -g -o build/soup
	gcc $(dbman_src) -Og -g -o build/dbman

release:
	gcc $(soup_src) -lfcgi -O2 -o build/soup
	gcc $(dbman_src) -O2 -o build/dbman


run:
	(cd www; REQUEST_URI=$(URI) ../build/soup)

debug_db:
	(cd www; REQUEST_URI=$(URI) $(db) ../build/soup)

debug_mem:
	(cd www; REQUEST_URI=$(URI) $(mem) ../build/soup)
