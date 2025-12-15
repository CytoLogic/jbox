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
} tail_args_t;


static void build_tail_argtable(tail_args_t *args) {
  args->help      = arg_lit0("h", "help", "display this help and exit");
  args->num_lines = arg_int0("n", NULL, "N",
                             "output the last N lines (default 10)");
  args->json      = arg_lit0(NULL, "json", "output in JSON format");
  args->file      = arg_file1(NULL, NULL, "FILE", "file to read");
  args->end       = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->num_lines;
  args->argtable[2] = args->json;
  args->argtable[3] = args->file;
  args->argtable[4] = args->end;
}


static void cleanup_tail_argtable(tail_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void tail_print_usage(FILE *out) {
  tail_args_t args;
  build_tail_argtable(&args);
  fprintf(out, "Usage: tail");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Print the last N lines of FILE to standard output.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_tail_argtable(&args);
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


// Read all lines from file into an array
static char **read_all_lines(FILE *fp, int *out_count) {
  char **lines = NULL;
  int capacity = 0;
  int count = 0;

  char *line = NULL;
  size_t len = 0;
  ssize_t nread;

  while ((nread = getline(&line, &len, fp)) != -1) {
    /* Check for interrupt */
    if (jbox_is_interrupted()) {
      free(line);
      for (int i = 0; i < count; i++) {
        free(lines[i]);
      }
      free(lines);
      *out_count = -1;  /* Signal interruption */
      return NULL;
    }

    // Remove trailing newline
    if (nread > 0 && line[nread - 1] == '\n') {
      line[nread - 1] = '\0';
    }

    if (count >= capacity) {
      capacity = capacity == 0 ? 16 : capacity * 2;
      char **new_lines = realloc(lines, capacity * sizeof(char *));
      if (!new_lines) {
        free(line);
        for (int i = 0; i < count; i++) {
          free(lines[i]);
        }
        free(lines);
        *out_count = 0;
        return NULL;
      }
      lines = new_lines;
    }

    lines[count] = strdup(line);
    if (!lines[count]) {
      free(line);
      for (int i = 0; i < count; i++) {
        free(lines[i]);
      }
      free(lines);
      *out_count = 0;
      return NULL;
    }
    count++;
  }

  free(line);
  *out_count = count;
  return lines;
}


static void free_lines(char **lines, int count) {
  for (int i = 0; i < count; i++) {
    free(lines[i]);
  }
  free(lines);
}


static int tail_file(const char *path, int num_lines, int show_json) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    if (show_json) {
      char escaped_path[512];
      escape_json_string(path, escaped_path, sizeof(escaped_path));
      printf("{\"path\": \"%s\", \"error\": \"%s\"}\n",
             escaped_path, strerror(errno));
    } else {
      fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
    }
    return 1;
  }

  int total_lines = 0;
  char **lines = read_all_lines(fp, &total_lines);
  fclose(fp);

  /* Check for interruption */
  if (!lines && total_lines == -1) {
    return 130;  /* 128 + SIGINT(2) */
  }

  if (!lines && total_lines != 0) {
    if (show_json) {
      char escaped_path[512];
      escape_json_string(path, escaped_path, sizeof(escaped_path));
      printf("{\"path\": \"%s\", \"error\": \"memory allocation failed\"}\n",
             escaped_path);
    } else {
      fprintf(stderr, "tail: %s: memory allocation failed\n", path);
    }
    return 1;
  }

  int start_line = total_lines > num_lines ? total_lines - num_lines : 0;

  if (show_json) {
    char escaped_path[512];
    escape_json_string(path, escaped_path, sizeof(escaped_path));
    printf("{\"path\": \"%s\", \"lines\": [", escaped_path);

    for (int i = start_line; i < total_lines; i++) {
      if (i > start_line) {
        printf(", ");
      }

      size_t line_len = strlen(lines[i]);
      size_t escaped_size = line_len * 2 + 1;
      if (escaped_size < 256) escaped_size = 256;
      char *escaped_line = malloc(escaped_size);
      if (escaped_line) {
        escape_json_string(lines[i], escaped_line, escaped_size);
        printf("\"%s\"", escaped_line);
        free(escaped_line);
      } else {
        printf("\"\"");
      }
    }

    printf("]}\n");
  } else {
    for (int i = start_line; i < total_lines; i++) {
      printf("%s\n", lines[i]);
    }
  }

  free_lines(lines, total_lines);
  return 0;
}


static int tail_run(int argc, char **argv) {
  tail_args_t args;
  build_tail_argtable(&args);

  /* Set up signal handler for clean interrupt */
  jbox_setup_sigint_handler();

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    tail_print_usage(stdout);
    cleanup_tail_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "tail");
    fprintf(stderr, "Try 'tail --help' for more information.\n");
    cleanup_tail_argtable(&args);
    return 1;
  }

  int num_lines = DEFAULT_LINES;
  if (args.num_lines->count > 0) {
    num_lines = args.num_lines->ival[0];
    if (num_lines < 0) {
      fprintf(stderr, "tail: invalid number of lines: %d\n", num_lines);
      cleanup_tail_argtable(&args);
      return 1;
    }
  }

  int show_json = args.json->count > 0;
  const char *path = args.file->filename[0];

  int result = tail_file(path, num_lines, show_json);

  cleanup_tail_argtable(&args);
  return result;
}


const jshell_cmd_spec_t cmd_tail_spec = {
  .name = "tail",
  .summary = "output the last part of files",
  .long_help = "Print the last N lines of FILE to standard output. "
               "With --json, outputs a JSON object with path and lines array.",
  .type = CMD_EXTERNAL,
  .run = tail_run,
  .print_usage = tail_print_usage
};


void jshell_register_tail_command(void) {
  jshell_register_command(&cmd_tail_spec);
}
