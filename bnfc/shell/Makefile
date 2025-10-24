BNFC ?= bnfc
GRAMMAR := Grammar.cf
.PHONY: all gen build clean run ast

all: build
gen:
	$(BNFC) --c --make $(GRAMMAR) -o build
build: gen
	$(MAKE) -C build all
run: build
	@echo "echo hello | grep h > out.txt" | ./Test
ast: build
	@echo "ls -l | grep cf" | ./Test
clean:
	$(MAKE) -C build distclean
