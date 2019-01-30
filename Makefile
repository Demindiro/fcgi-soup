CC        := gcc
CFLAGS    := -Wall -O0 -g -DNO_FCGI_DEFINES
OUTPUT    := build
OUTPUTBIN := $(OUTPUT)/soup
OUTPUTOBJ := $(OUTPUT)/obj

src := $(shell find . -name '*.c' ! -path '*test/*')
obj := $(src:./%.c=$(OUTPUTOBJ)/%.o)
includes := $(shell find . -name 'include' -type d)
includes := $(includes:./%=-I%)
lib = -lfcgi

cc_cmd = $(CC) $(CFLAGS) $(includes) $< -c -o $@
ld_cmd = $(CC) $(CFLAGS) $(lib) $(obj) -o $@
db = lldb
mem = valgrind


ifndef ROOT
	ROOT = www
endif
ifndef METHOD
	METHOD = GET
endif



build_debug: build/soup

build_release: build/soup


$(OUTPUTBIN): $(obj)
	@echo '    LD    $@'
	@$(ld_cmd)

$(OUTPUTOBJ)/%.o: %.c
	@echo '    CC    $@'
	@mkdir -p $(@D)
	@$(cc_cmd)



run:
	@cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) ../$(OUTPUTBIN)

run_db: build_debug
	@cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(db) $(DBFLAGS) ../$(OUTPUTBIN)

run_mem: build_debug
	@cd $(ROOT); REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(mem) --suppressions=../osx.supp $(DBFLAGS) ../$(OUTPUTBIN)
