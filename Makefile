CC        := gcc
CFLAGS    := -Wall -O0 -g -DNO_FCGI_DEFINES
OUTPUT    := build
OUTPUTBIN := $(OUTPUT)/soup
OUTPUTOBJ := $(OUTPUT)/obj
HTTP_HOST := example.org

src := $(shell find . -name '*.c' ! -path '*test/*')
obj := $(src:./%.c=$(OUTPUTOBJ)/%.o)
includes := $(shell find . -name 'include' -type d)
includes := $(includes:./%=-I%)
lib = -lfcgi

cc_cmd = $(CC) $(CFLAGS) $(includes) $< -c -o $@
ld_cmd = $(CC) $(CFLAGS) $(obj) $(lib) -o $@
db = lldb
mem = valgrind

env = HTTP_HOST=$(HTTP_HOST) REQUEST_METHOD=$(METHOD) PATH_INFO=$(URI) $(ENV)


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



run: build_debug
	@cd $(ROOT); $(env) ../$(OUTPUTBIN)

run_db: build_debug
	@cd $(ROOT); $(env) $(db) $(DBFLAGS) ../$(OUTPUTBIN)

run_mem: build_debug
	@cd $(ROOT); $(env) $(mem) --suppressions=../osx.supp $(DBFLAGS) ../$(OUTPUTBIN)
