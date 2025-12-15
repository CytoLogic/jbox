# Tests Makefile
# Note: Must be invoked from project root via `make -C tests`
PYTHON ?= python
PROJECT_ROOT := ..

.PHONY: all apps builtins jshell signals grammar ls stat cat head tail rg less vi \
        edit-replace-line edit-insert-line edit-delete-line edit-replace \
        jshell-path jshell-thread-exec jshell-pipes jshell-ast-exec jshell-session \
        jshell-signals app-signals vi-signals less-signals pkg-srv clean

all: apps builtins jshell

signals: jshell-signals app-signals vi-signals less-signals

apps: ls stat cat head tail rg less vi

builtins: edit-replace-line edit-insert-line edit-delete-line edit-replace

jshell: jshell-path jshell-thread-exec jshell-pipes jshell-ast-exec jshell-session

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

jshell-path:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.test_path -v

jshell-thread-exec:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.test_thread_exec -v

jshell-pipes:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.test_pipes -v

jshell-ast-exec:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.test_ast_exec -v

jshell-session:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.test_session -v

jshell-signals:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.jshell.test_signals -v

app-signals:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.test_signals -v

vi-signals:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.vi.test_vi_signals -v

less-signals:
	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.less.test_less_signals -v

clean:
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true
