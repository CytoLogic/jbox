# Compiler Settings
CC = gcc
CFLAGS = -Isrc -std=gnu23 -Wall -Werror -ggdb3 -fsanitize=address,undefined
COMPILE = $(CC) $(CFLAGS)

# Directories
PROJECT_ROOT := .
SRC_DIR := $(PROJECT_ROOT)/src
BIN_DIR := $(PROJECT_ROOT)/bin
EXTERN_DIR := $(PROJECT_ROOT)/extern

ARGTABLE3_DIR := $(EXTERN_DIR)/argtable3
ARGTABLE3_DIST := $(ARGTABLE3_DIR)/dist
ARGTABLE3_SRC := $(ARGTABLE3_DIST)/argtable3.c
ARGTABLE3_HDR := $(ARGTABLE3_DIST)/argtable3.h

all:
	$(COMPILE)

jbox:
	mkdir -p bin/
	$(COMPILE) -o $(BIN_DIR)/jbox /jbox src/jbox.c src/jsh.c src/utils/*
	ln -s jbox bin/cat
	ln -s jbox bin/cp
	ln -s jbox bin/echo
	ln -s jbox bin/ls
	ln -s jbox bin/mkdir
	ln -s jbox bin/mv
	ln -s jbox bin/pwd
	ln -s jbox bin/rm
	ln -s jbox bin/stat
	ln -s jbox bin/touch

$(ARGTABLE3_SRC) $(ARGTABLE3_HDR): argtable3-dist

argtable3-dist:
	@if [ ! -f $(ARGTABLE3_SRC) ]; then \
		cd $(ARGTABLE3_DIR)/tools && ./build dist; \
		cd ../../..; \
	fi

standalone:
	find src/utils-standalone -name "*.c"



clean:
	rm -rf $(BIN_DIR)/*
	rm -rf $(ARGTABLE3_DIST)
