#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_file *files;
  struct arg_end *end;
  void *argtable[4];
} cat_args_t;


static void build_cat_argtable(cat_args_t *args) {
  args->help  = arg_lit0("h", "help", "display this help and exit");
  args->json  = arg_lit0(NULL, "json", "output in JSON format");
  args->files = arg_filen(NULL, NULL, "FILE", 1, 100, "files to concatenate");
  args->end   = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->files;
  args->argtable[3] = args->end;
}


static void cleanup_cat_argtable(cat_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void cat_print_usage(FILE *out) {
  cat_args_t args;
  build_cat_argtable(&args);
  fprintf(out, "Usage: cat");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Concatenate FILE(s) to standard output.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_cat_argtable(&args);
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


static char *read_file_content(const char *path, size_t *out_size) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return NULL;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }

  long size = ftell(fp);
  if (size < 0) {
    fclose(fp);
    return NULL;
  }

  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return NULL;
  }

  char *content = malloc((size_t)size + 1);
  if (!content) {
    fclose(fp);
    return NULL;
  }

  size_t bytes_read = fread(content, 1, (size_t)size, fp);
  fclose(fp);

  content[bytes_read] = '\0';
  if (out_size) {
    *out_size = bytes_read;
  }

  return content;
}


static void print_json_content(const char *path, const char *content,
                               size_t content_size) {
  char escaped_path[512];
  escape_json_string(path, escaped_path, sizeof(escaped_path));

  size_t escaped_content_size = content_size * 2 + 1;
  char *escaped_content = malloc(escaped_content_size);
  if (!escaped_content) {
    printf("{\"path\": \"%s\", \"error\": \"memory allocation failed\"}\n",
           escaped_path);
    return;
  }

  escape_json_string(content, escaped_content, escaped_content_size);

  printf("{\"path\": \"%s\", \"content\": \"%s\"}",
         escaped_path, escaped_content);

  free(escaped_content);
}


static int cat_file(const char *path, int show_json, int *first_entry) {
  char *content = NULL;
  size_t content_size = 0;

  content = read_file_content(path, &content_size);
  if (!content) {
    if (show_json) {
      char escaped_path[512];
      escape_json_string(path, escaped_path, sizeof(escaped_path));
      if (!*first_entry) {
        printf(",\n");
      }
      *first_entry = 0;
      printf("{\"path\": \"%s\", \"error\": \"%s\"}",
             escaped_path, strerror(errno));
    } else {
      fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
    }
    return 1;
  }

  if (show_json) {
    if (!*first_entry) {
      printf(",\n");
    }
    *first_entry = 0;
    print_json_content(path, content, content_size);
  } else {
    fwrite(content, 1, content_size, stdout);
  }

  free(content);
  return 0;
}


static int cat_run(int argc, char **argv) {
  cat_args_t args;
  build_cat_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    cat_print_usage(stdout);
    cleanup_cat_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "cat");
    fprintf(stderr, "Try 'cat --help' for more information.\n");
    cleanup_cat_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  int first_entry = 1;
  int result = 0;

  if (show_json) {
    printf("[\n");
  }

  for (int i = 0; i < args.files->count; i++) {
    if (cat_file(args.files->filename[i], show_json, &first_entry) != 0) {
      result = 1;
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_cat_argtable(&args);
  return result;
}


const jshell_cmd_spec_t cmd_cat_spec = {
  .name = "cat",
  .summary = "concatenate files and print on standard output",
  .long_help = "Concatenate FILE(s) to standard output. "
               "With --json, outputs each file as a JSON object with "
               "path and content fields.",
  .type = CMD_EXTERNAL,
  .run = cat_run,
  .print_usage = cat_print_usage
};


void jshell_register_cat_command(void) {
  jshell_register_command(&cmd_cat_spec);
}
