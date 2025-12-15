CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23 -fsanitize=address,undefined
ARGTABLE_DIR = ../../../extern/argtable3/dist
SRC_DIR = ../../../src
CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
ARGTABLE_OBJ = $(ARGTABLE_DIR)/argtable3.o
REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c

OBJS = cmd_vi.o
LIB = libvi.a
BIN_DIR = ../../../bin/standalone-apps
BIN = $(BIN_DIR)/vi
PKG_BIN = $(BIN)

# Source files for package inclusion (for pkg compile after install)
PKG_SRCS = cmd_vi.c vi_main.c
PKG_HDRS = cmd_vi.h

all: $(BIN) $(LIB)

cmd_vi.o: cmd_vi.c cmd_vi.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): vi_main.o cmd_vi.o $(ARGTABLE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) vi_main.o cmd_vi.o $(REGISTRY_SRC) $(ARGTABLE_OBJ) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

include ../pkg.mk
