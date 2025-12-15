# Tests Makefile
# Note: Must be invoked from project root via `make -C tests`
PYTHON ?= python
PROJECT_ROOT := ..

.PHONY: all apps grammar ls stat cat head tail rg clean

all: apps

apps: ls stat cat head tail rg

grammar:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.grammar.test_grammar -v

ls:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.ls.test_ls -v

stat:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.stat.test_stat -v

cat:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.cat.test_cat -v

head:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.head.test_head -v

tail:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.tail.test_tail -v

rg:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.rg.test_rg -v

clean:
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true
