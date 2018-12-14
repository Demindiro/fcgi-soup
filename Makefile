proj_root = $(shell pwd)
soup_src  = src/*.c lib/string/src/* lib/template/src/*

db  = lldb
mem = valgrind


ifndef ROOT
	ROOT = www
endif
ifndef METHOD
	METHOD = GET
endif


build_debug:
	gcc $(soup_src) $(C_FLAGS) -lfcgi -Ilib/template/include -O0 -g -Wall -o build/soup

build_release:
	gcc $(soup_src) $(C_FLAGS) -lfcgi -Ilib/template/include -O2 -Wall -o build/soup


run:
	(cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(proj_root)/build/soup)

run_db: build_debug
	(cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(db) $(DB_FLAGS) $(proj_root)/build/soup)

run_mem: build_debug
	(cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(mem) $(DB_FLAGS) $(proj_root)/build/soup)
