CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23 -fsanitize=address,undefined
ARGTABLE_DIR = ../../../extern/argtable3/dist
SRC_DIR = ../../../src
CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
ARGTABLE_OBJ = $(ARGTABLE_DIR)/argtable3.o
REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c

OBJS = cmd_rmdir.o
LIB = librmdir.a
BIN_DIR = ../../../bin/standalone-apps
BIN = $(BIN_DIR)/rmdir

all: $(BIN) $(LIB)

cmd_rmdir.o: cmd_rmdir.c cmd_rmdir.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): rmdir_main.o cmd_rmdir.o $(ARGTABLE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) rmdir_main.o cmd_rmdir.o $(REGISTRY_SRC) $(ARGTABLE_OBJ) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)
