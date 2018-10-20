proj_root = $(shell pwd)
soup_src  = src/*.c
dbman_src = dbman/main.c src/article.c src/dictionary.c src/template.c src/database.c

db  = gdb
mem = valgrind


ifndef ROOT
	ROOT = www
endif


build_debug:
	gcc $(soup_src) -lfcgi -Og -g -o build/soup
	gcc $(dbman_src) -Og -g -o build/dbman

build_release:
	gcc $(soup_src) -lfcgi -O2 -o build/soup
	gcc $(dbman_src) -O2 -o build/dbman


run:
	(cd $(ROOT); REQUEST_URI=$(URI) $(proj_root)/build/soup)

run_db:
	(cd $(ROOT); REQUEST_URI=$(URI) $(db) $(proj_root)/build/soup)

run_mem:
	(cd $(ROOT); REQUEST_URI=$(URI) $(mem) $(proj_root)/build/soup)
