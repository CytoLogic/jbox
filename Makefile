# Compiler Settings
CC = gcc
CFLAGS = -Isrc -Igen/bnfc -std=gnu23 -Wall -Werror -ggdb3 -fsanitize=address,undefined
ifdef DEBUG
	CFLAGS += -DDEBUG -Wno-error=unused-variable -Wno-error=unused-function
endif
#LDFLAGS = 
COMPILE = $(CC) $(CFLAGS)


# Directories
PROJECT_ROOT := .
SRC_DIR := $(PROJECT_ROOT)/src
BIN_DIR := $(PROJECT_ROOT)/bin
EXTERN_DIR := $(PROJECT_ROOT)/extern

AST_DIR := $(SRC_DIR)/ast

ARGTABLE3_DIR := $(EXTERN_DIR)/argtable3
ARGTABLE3_DIST := $(ARGTABLE3_DIR)/dist
ARGTABLE3_SRC := $(ARGTABLE3_DIST)/argtable3.c
ARGTABLE3_HDR := $(ARGTABLE3_DIST)/argtable3.h

BNFC_GEN := $(PROJECT_ROOT)/gen/bnfc
BNFC_GRAMMAR := $(SRC_DIR)/shell-grammar/Grammar.cf
BNFC_HDRS := $(BNFC_GEN)/Absyn.h \
			 $(BNFC_GEN)/Parser.h \
			 $(BNFC_GEN)/Printer.h

BNFC_OBJS := $(BNFC_GEN)/Absyn.o \
			 $(BNFC_GEN)/Parser.o \
			 $(BNFC_GEN)/Lexer.o \
			 $(BNFC_GEN)/Printer.o
all:
	$(COMPILE)

jbox:
	mkdir -p bin/
	$(COMPILE) src/jbox.c src/jshell.c src/ast/*.c src/utils/* $(BNFC_OBJS) -o $(BIN_DIR)/jbox
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
	fi

bnfc:
	bnfc -m --c -o $(BNFC_GEN) $(BNFC_GRAMMAR)
	cd $(BNFC_GEN) && make

ast-traverser:
	cp -b $(BNFC_GEN)/Skeleton.c $(AST_DIR)/jshell_ast_interpreter.c
	cp -b $(BNFC_GEN)/Skeleton.h $(AST_DIR)/jshell_ast_interpreter.h
	sed -i 's/Skeleton.h/jshell_ast_traverser.h/g' $(AST_DIR)/jshell_ast_traverser.c
	sed -i 's/SKELETON_HEADER/JSHELL_AST_INTERPRETER_H/g' $(AST_DIR)/jshell_ast_traverser.h

standalone:
	find src/utils-standalone -name "*.c"



clean:
	rm -rf $(BIN_DIR)/*
	rm -rf $(ARGTABLE3_DIST)
	rm -rf $(BNFC_GEN)/*
