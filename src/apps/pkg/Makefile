CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23 -fsanitize=address,undefined
ARGTABLE_DIR = ../../../extern/argtable3/dist
SRC_DIR = ../../../src
CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
ARGTABLE_OBJ = $(ARGTABLE_DIR)/argtable3.o
REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c

OBJS = cmd_pkg.o pkg_utils.o pkg_json.o pkg_db.o pkg_registry.o
LIB = libpkg.a
BIN_DIR = ../../../bin/standalone-apps
BIN = $(BIN_DIR)/pkg
PKG_BIN = $(BIN)
LDFLAGS = -lm -lcurl

# Source files for package inclusion (for pkg compile after install)
PKG_SRCS = cmd_pkg.c pkg_main.c pkg_db.c pkg_json.c pkg_registry.c pkg_utils.c
PKG_HDRS = cmd_pkg.h pkg_db.h pkg_json.h pkg_registry.h pkg_utils.h

all: $(BIN) $(LIB)

cmd_pkg.o: cmd_pkg.c cmd_pkg.h pkg_utils.h pkg_json.h pkg_db.h pkg_registry.h
pkg_utils.o: pkg_utils.c pkg_utils.h
pkg_json.o: pkg_json.c pkg_json.h pkg_utils.h
pkg_db.o: pkg_db.c pkg_db.h pkg_utils.h
pkg_registry.o: pkg_registry.c pkg_registry.h pkg_json.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): pkg_main.o $(OBJS) $(ARGTABLE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) pkg_main.o $(OBJS) $(REGISTRY_SRC) $(ARGTABLE_OBJ) $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

include ../pkg.mk
