# Compiler Settings
CC = gcc
CFLAGS = -Isrc -Igen/bnfc -Iextern/argtable3/dist -std=gnu23 -Wall -Werror -ggdb3 -fsanitize=address,undefined
ifdef DEBUG
	CFLAGS += -DDEBUG -Wno-error=unused-variable -Wno-error=unused-function
endif
LDFLAGS = -lm
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
ARGTABLE3_OBJ := $(ARGTABLE3_DIST)/argtable3.o

BNFC_GEN := $(PROJECT_ROOT)/gen/bnfc
BNFC_GRAMMAR := $(SRC_DIR)/shell-grammar/Grammar.cf
BNFC_HDRS := $(BNFC_GEN)/Absyn.h \
			 $(BNFC_GEN)/Parser.h \
			 $(BNFC_GEN)/Printer.h

BNFC_OBJS := $(BNFC_GEN)/Absyn.o \
			 $(BNFC_GEN)/Parser.o \
			 $(BNFC_GEN)/Lexer.o \
			 $(BNFC_GEN)/Printer.o

JSHELL_SRCS := $(SRC_DIR)/jshell/jshell.c \
			   $(SRC_DIR)/jshell/jshell_cmd_registry.c \
			   $(SRC_DIR)/jshell/jshell_register_builtins.c \
			   $(SRC_DIR)/jshell/jshell_register_externals.c \
			   $(SRC_DIR)/jshell/jshell_job_control.c

BUILTIN_SRCS := $(SRC_DIR)/jshell/builtins/cmd_jobs.c \
				$(SRC_DIR)/jshell/builtins/cmd_ps.c \
				$(SRC_DIR)/jshell/builtins/cmd_kill.c \
				$(SRC_DIR)/jshell/builtins/cmd_wait.c \
				$(SRC_DIR)/jshell/builtins/cmd_edit_replace_line.c \
				$(SRC_DIR)/jshell/builtins/cmd_edit_insert_line.c \
				$(SRC_DIR)/jshell/builtins/cmd_edit_delete_line.c \
				$(SRC_DIR)/jshell/builtins/cmd_edit_replace.c

EXTERNAL_CMD_SRCS := $(SRC_DIR)/apps/ls/cmd_ls.c \
					 $(SRC_DIR)/apps/stat/cmd_stat.c \
					 $(SRC_DIR)/apps/cat/cmd_cat.c \
					 $(SRC_DIR)/apps/head/cmd_head.c \
					 $(SRC_DIR)/apps/tail/cmd_tail.c \
					 $(SRC_DIR)/apps/cp/cmd_cp.c \
					 $(SRC_DIR)/apps/mv/cmd_mv.c \
					 $(SRC_DIR)/apps/rm/cmd_rm.c \
					 $(SRC_DIR)/apps/mkdir/cmd_mkdir.c \
					 $(SRC_DIR)/apps/rmdir/cmd_rmdir.c \
					 $(SRC_DIR)/apps/touch/cmd_touch.c \
					 $(SRC_DIR)/apps/rg/cmd_rg.c

AST_SRCS := $(SRC_DIR)/ast/jshell_ast_interpreter.c \
			$(SRC_DIR)/ast/jshell_ast_helpers.c

all: jbox apps

.PHONY: test test-apps test-grammar
test: test-apps

test-apps: apps
	$(MAKE) -C tests apps

test-grammar:
	$(MAKE) -C tests grammar

apps: ls-app stat-app cat-app head-app tail-app cp-app mv-app rm-app mkdir-app rmdir-app touch-app rg-app

$(ARGTABLE3_OBJ): $(ARGTABLE3_SRC) $(ARGTABLE3_HDR)
	$(COMPILE) -c $(ARGTABLE3_SRC) -o $(ARGTABLE3_OBJ)

jbox: $(BNFC_OBJS) $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) src/jbox.c $(JSHELL_SRCS) $(BUILTIN_SRCS) $(EXTERNAL_CMD_SRCS) $(AST_SRCS) $(BNFC_OBJS) $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/jbox

ls-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/ls $(SRC_DIR)/apps/ls/ls_main.c \
	           $(SRC_DIR)/apps/ls/cmd_ls.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/ls

stat-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/stat $(SRC_DIR)/apps/stat/stat_main.c \
	           $(SRC_DIR)/apps/stat/cmd_stat.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/stat

cat-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/cat $(SRC_DIR)/apps/cat/cat_main.c \
	           $(SRC_DIR)/apps/cat/cmd_cat.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/cat

head-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/head $(SRC_DIR)/apps/head/head_main.c \
	           $(SRC_DIR)/apps/head/cmd_head.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/head

tail-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/tail $(SRC_DIR)/apps/tail/tail_main.c \
	           $(SRC_DIR)/apps/tail/cmd_tail.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/tail

cp-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/cp $(SRC_DIR)/apps/cp/cp_main.c \
	           $(SRC_DIR)/apps/cp/cmd_cp.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/cp

mv-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/mv $(SRC_DIR)/apps/mv/mv_main.c \
	           $(SRC_DIR)/apps/mv/cmd_mv.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/mv

rm-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/rm $(SRC_DIR)/apps/rm/rm_main.c \
	           $(SRC_DIR)/apps/rm/cmd_rm.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/rm

mkdir-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/mkdir $(SRC_DIR)/apps/mkdir/mkdir_main.c \
	           $(SRC_DIR)/apps/mkdir/cmd_mkdir.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/mkdir

rmdir-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/rmdir $(SRC_DIR)/apps/rmdir/rmdir_main.c \
	           $(SRC_DIR)/apps/rmdir/cmd_rmdir.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/rmdir

touch-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/touch $(SRC_DIR)/apps/touch/touch_main.c \
	           $(SRC_DIR)/apps/touch/cmd_touch.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/touch

rg-app: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) -I$(SRC_DIR)/apps/rg $(SRC_DIR)/apps/rg/rg_main.c \
	           $(SRC_DIR)/apps/rg/cmd_rg.c $(SRC_DIR)/jshell/jshell_cmd_registry.c \
	           $(ARGTABLE3_OBJ) $(LDFLAGS) -o $(BIN_DIR)/rg

$(ARGTABLE3_SRC) $(ARGTABLE3_HDR): argtable3-dist

argtable3-dist:
	@if [ ! -f $(ARGTABLE3_SRC) ]; then \
		cd $(ARGTABLE3_DIR)/tools && ./build dist; \
	fi

bnfc:
	bnfc -m --c -o $(BNFC_GEN) $(BNFC_GRAMMAR)
	cd $(BNFC_GEN) && make

new-ast-interpreter:
	cp -b $(BNFC_GEN)/Skeleton.c $(AST_DIR)/~jshell_ast_interpreter.c
	cp -b $(BNFC_GEN)/Skeleton.h $(AST_DIR)/~jshell_ast_interpreter.h
	sed -i 's/Skeleton.h/jshell_ast_interpreter.h/g' $(AST_DIR)/~jshell_ast_interpreter.c
	sed -i 's/SKELETON_HEADER/JSHELL_AST_INTERPRETER_H/g' $(AST_DIR)/~jshell_ast_interpreter.h

standalone:
	find src/utils-standalone -name "*.c"



clean:
	rm -rf $(BIN_DIR)/*
	rm -rf $(ARGTABLE3_DIST)
	rm -rf $(BNFC_GEN)/*
