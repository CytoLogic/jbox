CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23

# Detect build mode: installed (deps/ exists) vs source (project tree)
ifneq ($(wildcard deps/),)
  # Installed mode - use bundled dependencies
  # Compile argtable3.c from source (avoid ASAN mismatch from pre-built .o)
  BUILD_MODE = installed
  ARGTABLE_DIR = ./deps
  SRC_DIR = ./deps
  BIN_DIR = ./bin
  CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
  ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
  REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
else
  # Source mode - use project tree paths with sanitizers
  BUILD_MODE = source
  ARGTABLE_DIR = ../../../extern/argtable3/dist
  SRC_DIR = ../../../src
  BIN_DIR = ../../../bin/standalone-apps
  CFLAGS += -fsanitize=address,undefined
  CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
  ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.o
  REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
endif

OBJS = cmd_echo.o
LIB = libecho.a
BIN = $(BIN_DIR)/echo
PKG_BIN = $(BIN)

# Source files for package inclusion (for pkg compile after install)
PKG_SRCS = cmd_echo.c echo_main.c
PKG_HDRS = cmd_echo.h

all: $(BIN) $(LIB)
	@echo "Build mode: $(BUILD_MODE)"

cmd_echo.o: cmd_echo.c cmd_echo.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): echo_main.o cmd_echo.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) echo_main.o cmd_echo.o $(REGISTRY_SRC) $(ARGTABLE_SRC) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

# Only include pkg.mk in source mode (it references project paths)
ifeq ($(BUILD_MODE),source)
include pkg.mk
endif
