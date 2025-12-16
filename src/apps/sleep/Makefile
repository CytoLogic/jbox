CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23

ifneq ($(wildcard deps/),)
  BUILD_MODE = installed
  ARGTABLE_DIR = ./deps
  SRC_DIR = ./deps
  BIN_DIR = ./bin
  CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
  ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
  REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
else
  BUILD_MODE = source
  ARGTABLE_DIR = ../../../extern/argtable3/dist
  SRC_DIR = ../../../src
  BIN_DIR = ../../../bin/standalone-apps
  CFLAGS += -fsanitize=address,undefined
  CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
  ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.o
  REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
endif

OBJS = cmd_sleep.o
LIB = libsleep.a
BIN = $(BIN_DIR)/sleep
PKG_BIN = $(BIN)

all: $(BIN) $(LIB)
	@echo "Build mode: $(BUILD_MODE)"

cmd_sleep.o: cmd_sleep.c cmd_sleep.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): sleep_main.o cmd_sleep.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) sleep_main.o cmd_sleep.o $(REGISTRY_SRC) $(ARGTABLE_SRC) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

ifeq ($(BUILD_MODE),source)
include pkg.mk
endif
