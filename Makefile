proj_root = $(shell pwd)
soup_src  = src/*.c src/**/*.c

db  = lldb
mem = valgrind


ifndef ROOT
	ROOT = www
endif
ifndef METHOD
	METHOD = GET
endif


build_debug:
	gcc $(soup_src) $(C_FLAGS) -lfcgi -O0 -g -Wall -o build/soup

build_release:
	gcc $(soup_src) $(C_FLAGS) -lfcgi -O2 -Wall -o build/soup


run:
	(cd $(ROOT); METHOD=$(METHOD) PATH_INFO=$(URI) $(proj_root)/build/soup)

run_db:
	(cd $(ROOT); METHOD=$(METHOD) PATH_INFO=$(URI) $(db) $(DB_FLAGS) $(proj_root)/build/soup)

run_mem:
	(cd $(ROOT); method=$(METHOD) PATH_INFO=$(URI) $(mem) $(DB_FLAGS) $(proj_root)/build/soup)
