CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23 -fsanitize=address,undefined
ARGTABLE_DIR = ../../../extern/argtable3/dist
SRC_DIR = ../../../src
CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
ARGTABLE_OBJ = $(ARGTABLE_DIR)/argtable3.o
REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c

OBJS = cmd_mv.o
LIB = libmv.a
BIN_DIR = ../../../bin/standalone-apps
BIN = $(BIN_DIR)/mv
PKG_BIN = $(BIN)

# Source files for package inclusion (for pkg compile after install)
PKG_SRCS = cmd_mv.c mv_main.c
PKG_HDRS = cmd_mv.h

all: $(BIN) $(LIB)

cmd_mv.o: cmd_mv.c cmd_mv.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): mv_main.o cmd_mv.o $(ARGTABLE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) mv_main.o cmd_mv.o $(REGISTRY_SRC) $(ARGTABLE_OBJ) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

include ../pkg.mk
