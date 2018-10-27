proj_root = $(shell pwd)
soup_src  = src/*.c src/**/*.c
dbman_src = dbman/main.c src/article.c src/dictionary.c src/template.c src/database.c src/**/*.c

db  = lldb
mem = valgrind


ifndef ROOT
	ROOT = www
endif


build_debug:
	gcc $(soup_src) -lfcgi -O0 -g -Wall -o build/soup
	gcc $(dbman_src) -O0 -g -Wall -o build/dbman

build_release:
	gcc $(soup_src) -lfcgi -O2 -Wall -o build/soup
	gcc $(dbman_src) -O2 -Wall -o build/dbman


run:
	(cd $(ROOT); REQUEST_URI=$(URI) $(proj_root)/build/soup)

run_db:
	(cd $(ROOT); REQUEST_URI=$(URI) $(db) $(DB_FLAGS) $(proj_root)/build/soup)

run_mem:
	(cd $(ROOT); REQUEST_URI=$(URI) $(mem) $(DB_FLAGS) $(proj_root)/build/soup)
