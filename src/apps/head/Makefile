CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23 -fsanitize=address,undefined
ARGTABLE_DIR = ../../../extern/argtable3/dist
SRC_DIR = ../../../src
CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
ARGTABLE_OBJ = $(ARGTABLE_DIR)/argtable3.o
REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
SIGNALS_SRC = $(SRC_DIR)/utils/jbox_signals.c

OBJS = cmd_head.o
LIB = libhead.a
BIN_DIR = ../../../bin/standalone-apps
BIN = $(BIN_DIR)/head
PKG_BIN = $(BIN)

all: $(BIN) $(LIB)

cmd_head.o: cmd_head.c cmd_head.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): head_main.o cmd_head.o $(ARGTABLE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) head_main.o cmd_head.o $(REGISTRY_SRC) $(SIGNALS_SRC) $(ARGTABLE_OBJ) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

include ../pkg.mk
