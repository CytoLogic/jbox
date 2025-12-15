#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_file *files;
  struct arg_end *end;
  void *argtable[4];
} touch_args_t;


static void build_touch_argtable(touch_args_t *args) {
  args->help  = arg_lit0("h", "help", "display this help and exit");
  args->json  = arg_lit0(NULL, "json", "output in JSON format");
  args->files = arg_filen(NULL, NULL, "FILE", 1, 100, "files to create or "
                          "update");
  args->end   = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->files;
  args->argtable[3] = args->end;
}


static void cleanup_touch_argtable(touch_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void touch_print_usage(FILE *out) {
  touch_args_t args;
  build_touch_argtable(&args);
  fprintf(out, "Usage: touch");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Update the access and modification times of each FILE to the "
               "current time.\n");
  fprintf(out, "A FILE argument that does not exist is created empty.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_touch_argtable(&args);
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


static int touch_file(const char *path, int show_json, int *first_entry) {
  int result = 0;
  struct stat st;

  if (stat(path, &st) == 0) {
    if (utime(path, NULL) != 0) {
      result = -1;
    }
  } else {
    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
      result = -1;
    } else {
      close(fd);
    }
  }

  if (show_json) {
    char escaped_path[512];
    escape_json_string(path, escaped_path, sizeof(escaped_path));

    if (!*first_entry) {
      printf(",\n");
    }
    *first_entry = 0;

    if (result == 0) {
      printf("{\"path\": \"%s\", \"status\": \"ok\"}", escaped_path);
    } else {
      char escaped_error[256];
      escape_json_string(strerror(errno), escaped_error,
                         sizeof(escaped_error));
      printf("{\"path\": \"%s\", \"status\": \"error\", \"message\": \"%s\"}",
             escaped_path, escaped_error);
    }
  } else {
    if (result != 0) {
      fprintf(stderr, "touch: cannot touch '%s': %s\n", path, strerror(errno));
    }
  }

  return result;
}


static int touch_run(int argc, char **argv) {
  touch_args_t args;
  build_touch_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    touch_print_usage(stdout);
    cleanup_touch_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "touch");
    fprintf(stderr, "Try 'touch --help' for more information.\n");
    cleanup_touch_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  int first_entry = 1;
  int result = 0;

  if (show_json) {
    printf("[\n");
  }

  for (int i = 0; i < args.files->count; i++) {
    if (touch_file(args.files->filename[i], show_json, &first_entry) != 0) {
      result = 1;
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_touch_argtable(&args);
  return result;
}


const jshell_cmd_spec_t cmd_touch_spec = {
  .name = "touch",
  .summary = "change file timestamps",
  .long_help = "Update the access and modification times of each FILE to the "
               "current time. A FILE argument that does not exist is created "
               "empty.",
  .type = CMD_EXTERNAL,
  .run = touch_run,
  .print_usage = touch_print_usage
};


void jshell_register_touch_command(void) {
  jshell_register_command(&cmd_touch_spec);
}
