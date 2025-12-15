# Tests Makefile
# Note: Must be invoked from project root via `make -C tests`
PYTHON ?= python
PROJECT_ROOT := ..

.PHONY: all apps builtins grammar ls stat cat head tail rg less vi \
        edit-replace-line edit-insert-line edit-delete-line edit-replace \
        pkg-srv clean

all: apps builtins

apps: ls stat cat head tail rg less vi

builtins: edit-replace-line edit-insert-line edit-delete-line edit-replace

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

less:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.less.test_less -v

vi:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.vi.test_vi -v

edit-replace-line:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.builtins.test_edit_replace_line -v

edit-insert-line:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.builtins.test_edit_insert_line -v

edit-delete-line:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.builtins.test_edit_delete_line -v

edit-replace:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.builtins.test_edit_replace -v

pkg-srv:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.pkg_srv.test_pkg_srv -v

clean:
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true
