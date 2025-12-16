CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23 -fsanitize=address,undefined
ARGTABLE_DIR = ../../../extern/argtable3/dist
SRC_DIR = ../../../src
CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
ARGTABLE_OBJ = $(ARGTABLE_DIR)/argtable3.o
REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c

OBJS = cmd_ls.o
LIB = libls.a
BIN_DIR = ../../../bin/standalone-apps
BIN = $(BIN_DIR)/ls
PKG_BIN = $(BIN)

# Source files for package inclusion (for pkg compile after install)
PKG_SRCS = cmd_ls.c ls_main.c
PKG_HDRS = cmd_ls.h

all: $(BIN) $(LIB)

cmd_ls.o: cmd_ls.c cmd_ls.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): ls_main.o cmd_ls.o $(ARGTABLE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) ls_main.o cmd_ls.o $(REGISTRY_SRC) $(ARGTABLE_OBJ) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

include pkg.mk
