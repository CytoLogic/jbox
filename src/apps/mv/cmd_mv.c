#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *force;
  struct arg_lit *json;
  struct arg_file *source;
  struct arg_file *dest;
  struct arg_end *end;
  void *argtable[6];
} mv_args_t;


static void build_mv_argtable(mv_args_t *args) {
  args->help   = arg_lit0("h", "help", "display this help and exit");
  args->force  = arg_lit0("f", "force", "overwrite existing files");
  args->json   = arg_lit0(NULL, "json", "output in JSON format");
  args->source = arg_file1(NULL, NULL, "SOURCE", "source file or directory");
  args->dest   = arg_file1(NULL, NULL, "DEST", "destination path");
  args->end    = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->force;
  args->argtable[2] = args->json;
  args->argtable[3] = args->source;
  args->argtable[4] = args->dest;
  args->argtable[5] = args->end;
}


static void cleanup_mv_argtable(mv_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void mv_print_usage(FILE *out) {
  mv_args_t args;
  build_mv_argtable(&args);
  fprintf(out, "Usage: mv");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Rename SOURCE to DEST, or move SOURCE into DEST directory.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_mv_argtable(&args);
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


static int is_directory(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return 0;
  }
  return S_ISDIR(st.st_mode);
}


static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}


static char *build_dest_path(const char *src, const char *dest) {
  if (!is_directory(dest)) {
    return strdup(dest);
  }

  char *src_copy = strdup(src);
  if (!src_copy) {
    return NULL;
  }

  char *base = basename(src_copy);
  size_t len = strlen(dest) + strlen(base) + 2;
  char *result = malloc(len);

  if (result) {
    snprintf(result, len, "%s/%s", dest, base);
  }

  free(src_copy);
  return result;
}


static int mv_run(int argc, char **argv) {
  mv_args_t args;
  build_mv_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    mv_print_usage(stdout);
    cleanup_mv_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "mv");
    fprintf(stderr, "Try 'mv --help' for more information.\n");
    cleanup_mv_argtable(&args);
    return 1;
  }

  const char *source = args.source->filename[0];
  const char *dest = args.dest->filename[0];
  int force = args.force->count > 0;
  int show_json = args.json->count > 0;

  if (!file_exists(source)) {
    if (show_json) {
      char escaped_source[512];
      escape_json_string(source, escaped_source, sizeof(escaped_source));
      printf("{\"status\": \"error\", \"source\": \"%s\", "
             "\"message\": \"No such file or directory\"}\n", escaped_source);
    } else {
      fprintf(stderr, "mv: cannot stat '%s': No such file or directory\n",
              source);
    }
    cleanup_mv_argtable(&args);
    return 1;
  }

  char *final_dest = build_dest_path(source, dest);
  if (!final_dest) {
    if (show_json) {
      printf("{\"status\": \"error\", \"message\": \"memory allocation "
             "failed\"}\n");
    } else {
      fprintf(stderr, "mv: memory allocation failed\n");
    }
    cleanup_mv_argtable(&args);
    return 1;
  }

  if (!force && file_exists(final_dest)) {
    if (show_json) {
      char escaped_dest[512];
      escape_json_string(final_dest, escaped_dest, sizeof(escaped_dest));
      printf("{\"status\": \"error\", \"dest\": \"%s\", "
             "\"message\": \"File exists (use -f to overwrite)\"}\n",
             escaped_dest);
    } else {
      fprintf(stderr, "mv: '%s' already exists (use -f to overwrite)\n",
              final_dest);
    }
    free(final_dest);
    cleanup_mv_argtable(&args);
    return 1;
  }

  int result = rename(source, final_dest);

  if (show_json) {
    char escaped_source[512];
    char escaped_dest[512];
    escape_json_string(source, escaped_source, sizeof(escaped_source));
    escape_json_string(final_dest, escaped_dest, sizeof(escaped_dest));

    if (result == 0) {
      printf("{\"status\": \"ok\", \"source\": \"%s\", \"dest\": \"%s\"}\n",
             escaped_source, escaped_dest);
    } else {
      char escaped_error[256];
      escape_json_string(strerror(errno), escaped_error,
                         sizeof(escaped_error));
      printf("{\"status\": \"error\", \"source\": \"%s\", \"dest\": \"%s\", "
             "\"message\": \"%s\"}\n",
             escaped_source, escaped_dest, escaped_error);
    }
  } else {
    if (result != 0) {
      fprintf(stderr, "mv: cannot move '%s' to '%s': %s\n",
              source, final_dest, strerror(errno));
    }
  }

  free(final_dest);
  cleanup_mv_argtable(&args);
  return result == 0 ? 0 : 1;
}


const jshell_cmd_spec_t cmd_mv_spec = {
  .name = "mv",
  .summary = "move (rename) files",
  .long_help = "Rename SOURCE to DEST, or move SOURCE into DEST directory. "
               "With -f, overwrite existing destination files.",
  .type = CMD_EXTERNAL,
  .run = mv_run,
  .print_usage = mv_print_usage
};


void jshell_register_mv_command(void) {
  jshell_register_command(&cmd_mv_spec);
}
