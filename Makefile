soup_src  = src/*.c
dbman_src = dbman/main.c src/article.c src/dictionary.c src/template.c src/database.c



debug:
	gcc $(soup_src) -lfcgi -Og -g -o build/soup
	gcc $(dbman_src) -Og -g -o build/dbman

release:
	gcc $(soup_src) -lfcgi -O2 -o build/soup
	gcc $(dbman_src) -O2 -o build/dbman

run:
	echo $(URI)
	(cd www; REQUEST_URI=$(URI) ../build/soup)
