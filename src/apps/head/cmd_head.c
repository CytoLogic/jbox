#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "utils/jbox_signals.h"

#define DEFAULT_LINES 10


typedef struct {
  struct arg_lit *help;
  struct arg_int *num_lines;
  struct arg_lit *json;
  struct arg_file *file;
  struct arg_end *end;
  void *argtable[5];
} head_args_t;


static void build_head_argtable(head_args_t *args) {
  args->help      = arg_lit0("h", "help", "display this help and exit");
  args->num_lines = arg_int0("n", NULL, "N",
                             "output the first N lines (default 10)");
  args->json      = arg_lit0(NULL, "json", "output in JSON format");
  args->file      = arg_file0(NULL, NULL, "FILE", "file to read (stdin if omitted)");
  args->end       = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->num_lines;
  args->argtable[2] = args->json;
  args->argtable[3] = args->file;
  args->argtable[4] = args->end;
}


static void cleanup_head_argtable(head_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void head_print_usage(FILE *out) {
  head_args_t args;
  build_head_argtable(&args);
  fprintf(out, "Usage: head");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Print the first N lines of FILE to standard output.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_head_argtable(&args);
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


static int head_file(const char *path, int num_lines, int show_json) {
  FILE *fp;
  int is_stdin = (path == NULL || strcmp(path, "-") == 0);

  if (is_stdin) {
    fp = stdin;
    path = "<stdin>";
  } else {
    fp = fopen(path, "r");
    if (!fp) {
      if (show_json) {
        char escaped_path[512];
        escape_json_string(path, escaped_path, sizeof(escaped_path));
        printf("{\"path\": \"%s\", \"error\": \"%s\"}\n",
               escaped_path, strerror(errno));
      } else {
        fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
      }
      return 1;
    }
  }

  if (show_json) {
    char escaped_path[512];
    escape_json_string(path, escaped_path, sizeof(escaped_path));
    printf("{\"path\": \"%s\", \"lines\": [", escaped_path);
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t nread;
  int line_count = 0;
  int first_line = 1;

  while (line_count < num_lines && (nread = getline(&line, &len, fp)) != -1) {
    /* Check for interrupt */
    if (jbox_is_interrupted()) {
      free(line);
      if (!is_stdin) {
        fclose(fp);
      }
      if (show_json) {
        printf("]}\n");
      }
      return 130;  /* 128 + SIGINT(2) */
    }

    // Remove trailing newline for JSON output, keep for plain output
    size_t line_len = (size_t)nread;
    if (line_len > 0 && line[line_len - 1] == '\n') {
      line[line_len - 1] = '\0';
      line_len--;
    }

    if (show_json) {
      if (!first_line) {
        printf(", ");
      }
      first_line = 0;

      // Escape and print the line
      size_t escaped_size = line_len * 2 + 1;
      if (escaped_size < 256) escaped_size = 256;
      char *escaped_line = malloc(escaped_size);
      if (escaped_line) {
        escape_json_string(line, escaped_line, escaped_size);
        printf("\"%s\"", escaped_line);
        free(escaped_line);
      } else {
        printf("\"\"");
      }
    } else {
      printf("%s\n", line);
    }

    line_count++;
  }

  free(line);
  if (!is_stdin) {
    fclose(fp);
  }

  if (show_json) {
    printf("]}\n");
  }

  return 0;
}


static int head_run(int argc, char **argv) {
  head_args_t args;
  build_head_argtable(&args);

  /* Set up signal handler for clean interrupt */
  jbox_setup_sigint_handler();

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    head_print_usage(stdout);
    cleanup_head_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "head");
    fprintf(stderr, "Try 'head --help' for more information.\n");
    cleanup_head_argtable(&args);
    return 1;
  }

  int num_lines = DEFAULT_LINES;
  if (args.num_lines->count > 0) {
    num_lines = args.num_lines->ival[0];
    if (num_lines < 0) {
      fprintf(stderr, "head: invalid number of lines: %d\n", num_lines);
      cleanup_head_argtable(&args);
      return 1;
    }
  }

  int show_json = args.json->count > 0;
  const char *path = (args.file->count > 0) ? args.file->filename[0] : NULL;

  int result = head_file(path, num_lines, show_json);

  cleanup_head_argtable(&args);
  return result;
}


const jshell_cmd_spec_t cmd_head_spec = {
  .name = "head",
  .summary = "output the first part of files",
  .long_help = "Print the first N lines of FILE to standard output. "
               "With --json, outputs a JSON object with path and lines array.",
  .type = CMD_EXTERNAL,
  .run = head_run,
  .print_usage = head_print_usage
};


void jshell_register_head_command(void) {
  jshell_register_command(&cmd_head_spec);
}
