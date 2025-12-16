CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23

# Detect build mode: installed (deps/ exists) vs source (project tree)
ifneq ($(wildcard deps/),)
  BUILD_MODE = installed
  ARGTABLE_DIR = ./deps
  SRC_DIR = ./deps
  BIN_DIR = ./bin
  CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
  ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
  REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
  SIGNALS_SRC = $(SRC_DIR)/utils/jbox_signals.c
else
  BUILD_MODE = source
  ARGTABLE_DIR = ../../../extern/argtable3/dist
  SRC_DIR = ../../../src
  BIN_DIR = ../../../bin/standalone-apps
  CFLAGS += -fsanitize=address,undefined
  CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
  ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.o
  REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
  SIGNALS_SRC = $(SRC_DIR)/utils/jbox_signals.c
endif

OBJS = cmd_ftp.o ftp_client.o ftp_interactive.o
LIB = libftp.a
BIN = $(BIN_DIR)/ftp
PKG_BIN = $(BIN)

all: $(BIN) $(LIB)
	@echo "Build mode: $(BUILD_MODE)"

cmd_ftp.o: cmd_ftp.c cmd_ftp.h ftp_client.h ftp_interactive.h

ftp_client.o: ftp_client.c ftp_client.h

ftp_interactive.o: ftp_interactive.c ftp_interactive.h ftp_client.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): ftp_main.o $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) ftp_main.o $(OBJS) $(REGISTRY_SRC) $(SIGNALS_SRC) $(ARGTABLE_SRC) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

ifeq ($(BUILD_MODE),source)
include pkg.mk
endif
