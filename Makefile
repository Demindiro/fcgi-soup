proj_root = $(shell pwd)
src     = src/*.c lib/template/src/*.c lib/template/lib/*/src/*.c lib/template/src/*/*.c
headers = -Ilib/template/include -Ilib/template/lib/string/include -Ilib/template/lib/temp-alloc/include

db  = lldb
mem = valgrind


ifndef ROOT
	ROOT = www
endif
ifndef METHOD
	METHOD = GET
endif


build_debug:
	gcc $(src) $(C_FLAGS) -lfcgi $(headers) -O0 -g -Wall -o build/soup

build_release:
	gcc $(src) $(C_FLAGS) -lfcgi $(headers) -O2 -Wall -o build/soup


run:
	(cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(proj_root)/build/soup)

run_db: build_debug
	(cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(db) $(DB_FLAGS) $(proj_root)/build/soup)

run_mem: build_debug
	(cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(mem) $(DB_FLAGS) $(proj_root)/build/soup)
