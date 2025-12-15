#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_file *file;
  struct arg_int *line_num;
  struct arg_str *text;
  struct arg_end *end;
  void *argtable[6];
} edit_insert_line_args_t;


static void build_edit_insert_line_argtable(edit_insert_line_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->file = arg_file1(NULL, NULL, "FILE", "file to edit");
  args->line_num = arg_int1(NULL, NULL, "LINE",
                            "line number to insert before (1-based)");
  args->text = arg_str1(NULL, NULL, "TEXT", "text to insert");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->file;
  args->argtable[3] = args->line_num;
  args->argtable[4] = args->text;
  args->argtable[5] = args->end;
}


static void cleanup_edit_insert_line_argtable(edit_insert_line_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void edit_insert_line_print_usage(FILE *out) {
  edit_insert_line_args_t args;
  build_edit_insert_line_argtable(&args);
  fprintf(out, "Usage: edit-insert-line");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Insert a line before a given line number in a file.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_edit_insert_line_argtable(&args);
}


static void escape_json_string(const char *str, char *out, size_t out_size) {
  size_t j = 0;
  for (size_t i = 0; str[i] && j < out_size - 1; i++) {
    char c = str[i];
    if (c == '"' || c == '\\') {
      if (j + 2 >= out_size) break;
      out[j++] = '\\';
      out[j++] = c;
    } else if (c == '\n') {
      if (j + 2 >= out_size) break;
      out[j++] = '\\';
      out[j++] = 'n';
    } else if (c == '\r') {
      if (j + 2 >= out_size) break;
      out[j++] = '\\';
      out[j++] = 'r';
    } else if (c == '\t') {
      if (j + 2 >= out_size) break;
      out[j++] = '\\';
      out[j++] = 't';
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}


static void print_json_result(const char *path, int line, const char *status,
                              const char *message) {
  char escaped_path[512];
  escape_json_string(path, escaped_path, sizeof(escaped_path));

  if (message) {
    char escaped_msg[512];
    escape_json_string(message, escaped_msg, sizeof(escaped_msg));
    printf("{\"path\": \"%s\", \"line\": %d, \"status\": \"%s\", "
           "\"message\": \"%s\"}\n",
           escaped_path, line, status, escaped_msg);
  } else {
    printf("{\"path\": \"%s\", \"line\": %d, \"status\": \"%s\"}\n",
           escaped_path, line, status);
  }
}


typedef struct {
  char **lines;
  size_t count;
  size_t capacity;
} line_buffer_t;


static void line_buffer_init(line_buffer_t *buf) {
  buf->lines = NULL;
  buf->count = 0;
  buf->capacity = 0;
}


static void line_buffer_free(line_buffer_t *buf) {
  for (size_t i = 0; i < buf->count; i++) {
    free(buf->lines[i]);
  }
  free(buf->lines);
  buf->lines = NULL;
  buf->count = 0;
  buf->capacity = 0;
}


static int line_buffer_add(line_buffer_t *buf, const char *line) {
  if (buf->count >= buf->capacity) {
    size_t new_cap = buf->capacity == 0 ? 256 : buf->capacity * 2;
    char **new_lines = realloc(buf->lines, new_cap * sizeof(char *));
    if (!new_lines) return -1;
    buf->lines = new_lines;
    buf->capacity = new_cap;
  }
  buf->lines[buf->count] = strdup(line);
  if (!buf->lines[buf->count]) return -1;
  buf->count++;
  return 0;
}


static int line_buffer_insert(line_buffer_t *buf, size_t index,
                              const char *line) {
  if (buf->count >= buf->capacity) {
    size_t new_cap = buf->capacity == 0 ? 256 : buf->capacity * 2;
    char **new_lines = realloc(buf->lines, new_cap * sizeof(char *));
    if (!new_lines) return -1;
    buf->lines = new_lines;
    buf->capacity = new_cap;
  }

  for (size_t i = buf->count; i > index; i--) {
    buf->lines[i] = buf->lines[i - 1];
  }

  buf->lines[index] = strdup(line);
  if (!buf->lines[index]) return -1;
  buf->count++;
  return 0;
}


static int read_file_lines(const char *path, line_buffer_t *buf) {
  FILE *fp = fopen(path, "r");
  if (!fp) return -1;

  char *line = NULL;
  size_t line_cap = 0;
  ssize_t line_len;

  while ((line_len = getline(&line, &line_cap, fp)) != -1) {
    if (line_len > 0 && line[line_len - 1] == '\n') {
      line[line_len - 1] = '\0';
    }
    if (line_buffer_add(buf, line) != 0) {
      free(line);
      fclose(fp);
      return -1;
    }
  }

  free(line);
  fclose(fp);
  return 0;
}


static int write_file_lines(const char *path, line_buffer_t *buf) {
  FILE *fp = fopen(path, "w");
  if (!fp) return -1;

  for (size_t i = 0; i < buf->count; i++) {
    if (fprintf(fp, "%s\n", buf->lines[i]) < 0) {
      fclose(fp);
      return -1;
    }
  }

  fclose(fp);
  return 0;
}


static int edit_insert_line_run(int argc, char **argv) {
  edit_insert_line_args_t args;
  build_edit_insert_line_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    edit_insert_line_print_usage(stdout);
    cleanup_edit_insert_line_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "edit-insert-line");
    fprintf(stderr, "Try 'edit-insert-line --help' for more information.\n");
    cleanup_edit_insert_line_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  const char *filepath = args.file->filename[0];
  int line_num = args.line_num->ival[0];
  const char *text = args.text->sval[0];

  if (line_num < 1) {
    if (show_json) {
      print_json_result(filepath, line_num, "error",
                        "line number must be >= 1");
    } else {
      fprintf(stderr, "edit-insert-line: line number must be >= 1\n");
    }
    cleanup_edit_insert_line_argtable(&args);
    return 1;
  }

  line_buffer_t lines;
  line_buffer_init(&lines);

  if (read_file_lines(filepath, &lines) != 0) {
    if (show_json) {
      print_json_result(filepath, line_num, "error", strerror(errno));
    } else {
      fprintf(stderr, "edit-insert-line: %s: %s\n", filepath, strerror(errno));
    }
    line_buffer_free(&lines);
    cleanup_edit_insert_line_argtable(&args);
    return 1;
  }

  if ((size_t)line_num > lines.count + 1) {
    if (show_json) {
      print_json_result(filepath, line_num, "error",
                        "line number exceeds file length + 1");
    } else {
      fprintf(stderr, "edit-insert-line: line %d exceeds file length + 1 "
              "(%zu)\n", line_num, lines.count + 1);
    }
    line_buffer_free(&lines);
    cleanup_edit_insert_line_argtable(&args);
    return 1;
  }

  if (line_buffer_insert(&lines, (size_t)(line_num - 1), text) != 0) {
    if (show_json) {
      print_json_result(filepath, line_num, "error", "memory allocation failed");
    } else {
      fprintf(stderr, "edit-insert-line: memory allocation failed\n");
    }
    line_buffer_free(&lines);
    cleanup_edit_insert_line_argtable(&args);
    return 1;
  }

  if (write_file_lines(filepath, &lines) != 0) {
    if (show_json) {
      print_json_result(filepath, line_num, "error", strerror(errno));
    } else {
      fprintf(stderr, "edit-insert-line: failed to write %s: %s\n",
              filepath, strerror(errno));
    }
    line_buffer_free(&lines);
    cleanup_edit_insert_line_argtable(&args);
    return 1;
  }

  if (show_json) {
    print_json_result(filepath, line_num, "ok", NULL);
  }

  line_buffer_free(&lines);
  cleanup_edit_insert_line_argtable(&args);
  return 0;
}


const jshell_cmd_spec_t cmd_edit_insert_line_spec = {
  .name = "edit-insert-line",
  .summary = "insert a line before a given line number",
  .long_help = "Insert TEXT before line LINE in FILE. "
               "Line numbers are 1-based. Use line_count+1 to append.",
  .type = CMD_BUILTIN,
  .run = edit_insert_line_run,
  .print_usage = edit_insert_line_print_usage
};


void jshell_register_edit_insert_line_command(void) {
  jshell_register_command(&cmd_edit_insert_line_spec);
}
