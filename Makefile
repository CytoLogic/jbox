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

CURL_DIR := $(EXTERN_DIR)/curl
CURL_BUILD := $(CURL_DIR)/build
CURL_LIB := $(CURL_BUILD)/lib/libcurl.a
CURL_INCLUDE := $(CURL_DIR)/include
CURL_CFLAGS := -I$(CURL_INCLUDE) -I$(CURL_BUILD)/include/curl
CURL_LDFLAGS := $(CURL_LIB) -lssl -lcrypto -lz -lpthread -lidn2

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
			   $(SRC_DIR)/jshell/jshell_pkg_loader.c \
			   $(SRC_DIR)/jshell/jshell_job_control.c \
			   $(SRC_DIR)/jshell/jshell_history.c \
			   $(SRC_DIR)/jshell/jshell_path.c \
			   $(SRC_DIR)/jshell/jshell_env_loader.c \
			   $(SRC_DIR)/jshell/jshell_thread_exec.c \
			   $(SRC_DIR)/jshell/jshell_socketpair.c \
			   $(SRC_DIR)/jshell/jshell_signals.c \
			   $(SRC_DIR)/jshell/jshell_ai.c \
			   $(SRC_DIR)/jshell/jshell_ai_context.c \
			   $(SRC_DIR)/jshell/jshell_gemini_api.c \
			   $(SRC_DIR)/utils/jbox_signals.c

BUILTIN_SRCS := $(SRC_DIR)/jshell/builtins/cmd_jobs.c \
				$(SRC_DIR)/jshell/builtins/cmd_ps.c \
				$(SRC_DIR)/jshell/builtins/cmd_kill.c \
				$(SRC_DIR)/jshell/builtins/cmd_wait.c \
				$(SRC_DIR)/jshell/builtins/cmd_edit_replace_line.c \
				$(SRC_DIR)/jshell/builtins/cmd_edit_insert_line.c \
				$(SRC_DIR)/jshell/builtins/cmd_edit_delete_line.c \
				$(SRC_DIR)/jshell/builtins/cmd_edit_replace.c \
				$(SRC_DIR)/jshell/builtins/cmd_cd.c \
				$(SRC_DIR)/jshell/builtins/cmd_pwd.c \
				$(SRC_DIR)/jshell/builtins/cmd_env.c \
				$(SRC_DIR)/jshell/builtins/cmd_export.c \
				$(SRC_DIR)/jshell/builtins/cmd_unset.c \
				$(SRC_DIR)/jshell/builtins/cmd_type.c \
				$(SRC_DIR)/jshell/builtins/cmd_help.c \
				$(SRC_DIR)/jshell/builtins/cmd_history.c \
				$(SRC_DIR)/jshell/builtins/cmd_http_get.c \
				$(SRC_DIR)/jshell/builtins/cmd_http_post.c

# Only pkg command is linked into jshell. Other external commands (ls, cat, etc.)
# are built as standalone binaries and registered dynamically via pkg install.
EXTERNAL_CMD_SRCS := $(SRC_DIR)/apps/pkg/cmd_pkg.c \
					 $(SRC_DIR)/apps/pkg/pkg_utils.c \
					 $(SRC_DIR)/apps/pkg/pkg_db.c \
					 $(SRC_DIR)/apps/pkg/pkg_json.c \
					 $(SRC_DIR)/apps/pkg/pkg_registry.c

AST_SRCS := $(SRC_DIR)/ast/jshell_ast_interpreter.c \
			$(SRC_DIR)/ast/jshell_ast_helpers.c

# FTP server sources
FTPD_DIR := $(SRC_DIR)/ftpd
FTPD_SRCS := $(FTPD_DIR)/ftpd.c \
			 $(FTPD_DIR)/ftpd_main.c \
			 $(FTPD_DIR)/ftpd_client.c \
			 $(FTPD_DIR)/ftpd_commands.c \
			 $(FTPD_DIR)/ftpd_data.c \
			 $(FTPD_DIR)/ftpd_path.c

all: jbox apps packages ftpd

.PHONY: test test-apps test-builtins test-grammar test-pkg-srv apps clean-apps bnfc packages clean-packages clean-pkg-repository ftpd clean-ftpd
test: test-apps test-builtins test-pkg-srv

test-apps: apps
	$(MAKE) -C tests apps

test-builtins: jbox
	$(MAKE) -C tests builtins

test-grammar:
	$(MAKE) -C tests grammar

test-pkg-srv:
	$(MAKE) -C tests pkg-srv

APP_DIRS := cat cp date echo ftp head less ls mkdir mv pkg rg rm rmdir sleep stat tail touch vi

apps: $(ARGTABLE3_OBJ)
	@for app in $(APP_DIRS); do \
		echo "Building $$app..."; \
		$(MAKE) -C $(SRC_DIR)/apps/$$app; \
	done

clean-apps:
	@for app in $(APP_DIRS); do \
		$(MAKE) -C $(SRC_DIR)/apps/$$app clean; \
	done

# Package building
packages: apps
	@for app in $(APP_DIRS); do \
		echo "Packaging $$app..."; \
		$(MAKE) -C $(SRC_DIR)/apps/$$app pkg; \
	done
	@./scripts/generate-pkg-manifest.sh
	@echo "All packages built to srv/pkg_repository/downloads/"

clean-packages:
	@for app in $(APP_DIRS); do \
		$(MAKE) -C $(SRC_DIR)/apps/$$app pkg-clean; \
	done

clean-pkg-repository:
	rm -rf srv/pkg_repository/downloads/*
	rm -f srv/pkg_repository/pkg_manifest.json

$(ARGTABLE3_OBJ): $(ARGTABLE3_SRC) $(ARGTABLE3_HDR)
	$(COMPILE) -c $(ARGTABLE3_SRC) -o $(ARGTABLE3_OBJ)

$(BNFC_OBJS): bnfc

jbox: $(BNFC_OBJS) $(ARGTABLE3_OBJ) $(CURL_LIB)
	mkdir -p bin/
	$(COMPILE) $(CURL_CFLAGS) src/jbox.c $(JSHELL_SRCS) $(BUILTIN_SRCS) $(EXTERNAL_CMD_SRCS) $(AST_SRCS) $(BNFC_OBJS) $(ARGTABLE3_OBJ) $(CURL_LDFLAGS) $(LDFLAGS) -o $(BIN_DIR)/jbox
	ln -sf jbox $(BIN_DIR)/jshell

$(ARGTABLE3_SRC) $(ARGTABLE3_HDR): argtable3-dist

argtable3-dist:
	@if [ ! -f $(ARGTABLE3_SRC) ]; then \
		cd $(ARGTABLE3_DIR)/tools && ./build dist; \
	fi

$(CURL_LIB): curl-build

curl-build:
	@if [ ! -f $(CURL_LIB) ]; then \
		mkdir -p $(CURL_BUILD) && \
		cd $(CURL_BUILD) && \
		cmake .. -DBUILD_SHARED_LIBS=OFF \
		         -DBUILD_CURL_EXE=OFF \
		         -DBUILD_TESTING=OFF \
		         -DCURL_USE_OPENSSL=ON \
		         -DHTTP_ONLY=ON \
		         -DCURL_DISABLE_LDAP=ON \
		         -DCURL_DISABLE_LDAPS=ON \
		         -DCURL_USE_LIBSSH2=OFF \
		         -DCURL_USE_LIBSSH=OFF \
		         -DUSE_NGHTTP2=OFF \
		         -DCURL_USE_LIBPSL=OFF \
		         -DCURL_DISABLE_ALTSVC=ON \
		         -DCURL_DISABLE_HSTS=ON \
		         -DCURL_ZSTD=OFF \
		         -DCURL_BROTLI=OFF \
		         -DCURL_USE_LIBIDN2=OFF && \
		make -j$$(nproc); \
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



# FTP server daemon
ftpd: $(ARGTABLE3_OBJ)
	mkdir -p $(BIN_DIR)
	$(COMPILE) $(FTPD_SRCS) $(ARGTABLE3_OBJ) -lpthread $(LDFLAGS) -o $(BIN_DIR)/ftpd

clean-ftpd:
	rm -f $(BIN_DIR)/ftpd

clean: clean-pkg-repository clean-ftpd
	rm -rf $(BIN_DIR)/*
	rm -f $(BIN_DIR)/jshell
	rm -rf $(ARGTABLE3_DIST)
	rm -rf $(CURL_BUILD)
	rm -rf $(BNFC_GEN)/*
