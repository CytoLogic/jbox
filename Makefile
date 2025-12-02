CC = gcc
CFLAGS = -Isrc -std=gnu23 -Wall -Werror -ggdb3 -fsanitize=address,undefined
COMPILE = $(CC) $(CFLAGS)

all:
	$(COMPILE)

jbox:
	$(COMPILE) -o bin/jbox src/jbox.c src/jsh.c src/utils/*
	ln -s jbox bin/cat
	ln -s jbox bin/cp
	ln -s jbox bin/echo
	ln -s jbox bin/ls
	ln -s jbox bin/mkdir
	ln -s jbox bin/mv
	ln -s jbox bin/pwd
	ln -s jbox bin/rm
	ln -s jbox bin/stat
	ln -s jbox bin/touch

standalone:
	find src/utils-standalone -name "*.c"


clean:
	rm bin/*
