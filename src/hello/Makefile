CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -largtable3

OBJS = cmd_hello.o registry.o
LIB = libhello.a
BIN = hello

all: $(BIN) $(LIB)

cmd_hello.o: cmd_hello.c cmd_spec.h
registry.o: registry.c cmd_spec.h

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): hello_main.o cmd_hello.o
	$(CC) $(CFLAGS) -o $(BIN) hello_main.o cmd_hello.o $(LDFLAGS)

clean:
	rm -f *.o $(BIN) $(LIB)

