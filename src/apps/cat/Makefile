CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23 -fsanitize=address,undefined
ARGTABLE_DIR = ../../../extern/argtable3/dist
SRC_DIR = ../../../src
CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
ARGTABLE_OBJ = $(ARGTABLE_DIR)/argtable3.o
REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
SIGNALS_SRC = $(SRC_DIR)/utils/jbox_signals.c

OBJS = cmd_cat.o
LIB = libcat.a
BIN_DIR = ../../../bin/standalone-apps
BIN = $(BIN_DIR)/cat
PKG_BIN = $(BIN)

# Source files for package inclusion (for pkg compile after install)
PKG_SRCS = cmd_cat.c cat_main.c
PKG_HDRS = cmd_cat.h

all: $(BIN) $(LIB)

cmd_cat.o: cmd_cat.c cmd_cat.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): cat_main.o cmd_cat.o $(ARGTABLE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) cat_main.o cmd_cat.o $(REGISTRY_SRC) $(SIGNALS_SRC) $(ARGTABLE_OBJ) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

include pkg.mk
