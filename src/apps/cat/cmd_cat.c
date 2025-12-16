/**
 * @file cmd_cat.c
 * @brief Implementation of the cat command for concatenating files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "utils/jbox_signals.h"


/**
 * Arguments structure for the cat command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_file *files;
  struct arg_end *end;
  void *argtable[4];
} cat_args_t;


/**
 * Initializes the argtable3 argument definitions for the cat command.
 *
 * @param args Pointer to the cat_args_t structure to initialize.
 */
static void build_cat_argtable(cat_args_t *args) {
  args->help  = arg_lit0("h", "help", "display this help and exit");
  args->json  = arg_lit0(NULL, "json", "output in JSON format");
  args->files = arg_filen(NULL, NULL, "FILE", 0, 100,
                          "files to concatenate (stdin if omitted)");
  args->end   = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->files;
  args->argtable[3] = args->end;
}


/**
 * Frees memory allocated by build_cat_argtable.
 *
 * @param args Pointer to the cat_args_t structure to clean up.
 */
static void cleanup_cat_argtable(cat_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the cat command.
 *
 * @param out File stream to write usage information to.
 */
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


/**
 * Escapes special characters in a string for JSON output.
 *
 * @param str     The input string to escape.
 * @param out     Buffer to write the escaped string to.
 * @param out_size Size of the output buffer.
 */
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


/**
 * Reads entire content from a file stream into a dynamically allocated buffer.
 *
 * @param fp       File pointer to read from.
 * @param out_size Optional pointer to receive the number of bytes read.
 * @return Newly allocated buffer containing the content, or NULL on error.
 *         Caller must free the returned buffer.
 */
static char *read_stream_content(FILE *fp, size_t *out_size) {
  size_t capacity = 4096;
  size_t total = 0;
  char *content = malloc(capacity);
  if (!content) {
    return NULL;
  }

  while (1) {
    if (total + 4096 > capacity) {
      capacity *= 2;
      char *new_content = realloc(content, capacity);
      if (!new_content) {
        free(content);
        return NULL;
      }
      content = new_content;
    }

    size_t bytes_read = fread(content + total, 1, 4096, fp);
    total += bytes_read;
    if (bytes_read < 4096) {
      break;
    }
  }

  content[total] = '\0';
  if (out_size) {
    *out_size = total;
  }

  return content;
}


/**
 * Reads entire content of a file into a dynamically allocated buffer.
 *
 * Attempts to determine file size first for efficiency, but falls back
 * to streaming read for special files (e.g., /proc/* files).
 *
 * @param path     Path to the file to read.
 * @param out_size Optional pointer to receive the number of bytes read.
 * @return Newly allocated buffer containing the content, or NULL on error.
 *         Caller must free the returned buffer.
 */
static char *read_file_content(const char *path, size_t *out_size) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return NULL;
  }

  /* Try to get file size via seeking, fall back to streaming read */
  int can_seek = (fseek(fp, 0, SEEK_END) == 0);
  long size = can_seek ? ftell(fp) : -1;

  if (can_seek && size > 0) {
    /* Regular file with known size */
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

  /* Special file (e.g., /proc/*) or empty file - use streaming read */
  if (can_seek) {
    fseek(fp, 0, SEEK_SET);
  }

  char *content = read_stream_content(fp, out_size);
  fclose(fp);
  return content;
}


/**
 * Prints file content as a JSON object with path and content fields.
 *
 * @param path         Path to the file (for JSON output).
 * @param content      Content of the file.
 * @param content_size Size of the content in bytes.
 */
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


/**
 * Reads and outputs a single file's content.
 *
 * @param path        Path to the file, or NULL/"-" for stdin.
 * @param show_json   If non-zero, output in JSON format.
 * @param first_entry Pointer to flag tracking if this is the first JSON entry.
 * @return 0 on success, 1 on error.
 */
static int cat_file(const char *path, int show_json, int *first_entry) {
  char *content = NULL;
  size_t content_size = 0;
  int is_stdin = (path == NULL || strcmp(path, "-") == 0);
  const char *display_path = is_stdin ? "<stdin>" : path;

  if (is_stdin) {
    content = read_stream_content(stdin, &content_size);
  } else {
    content = read_file_content(path, &content_size);
  }

  if (!content) {
    if (show_json) {
      char escaped_path[512];
      escape_json_string(display_path, escaped_path, sizeof(escaped_path));
      if (!*first_entry) {
        printf(",\n");
      }
      *first_entry = 0;
      printf("{\"path\": \"%s\", \"error\": \"%s\"}",
             escaped_path, strerror(errno));
    } else {
      fprintf(stderr, "cat: %s: %s\n", display_path, strerror(errno));
    }
    return 1;
  }

  if (show_json) {
    if (!*first_entry) {
      printf(",\n");
    }
    *first_entry = 0;
    print_json_content(display_path, content, content_size);
  } else {
    fwrite(content, 1, content_size, stdout);
  }

  free(content);
  return 0;
}


/**
 * Main entry point for the cat command.
 *
 * Parses arguments and concatenates specified files to stdout.
 * Supports JSON output format with --json flag.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, non-zero on error.
 */
static int cat_run(int argc, char **argv) {
  cat_args_t args;
  build_cat_argtable(&args);

  /* Set up signal handler for clean interrupt */
  jbox_setup_sigint_handler();

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

  if (args.files->count == 0) {
    /* No files specified, read from stdin */
    if (cat_file(NULL, show_json, &first_entry) != 0) {
      result = 1;
    }
  } else {
    for (int i = 0; i < args.files->count; i++) {
      /* Check for interrupt between files */
      if (jbox_is_interrupted()) {
        result = 130;  /* 128 + SIGINT(2) */
        break;
      }
      if (cat_file(args.files->filename[i], show_json, &first_entry) != 0) {
        result = 1;
      }
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_cat_argtable(&args);
  return result;
}


/**
 * Command specification for the cat command.
 */
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


/**
 * Registers the cat command with the shell command registry.
 */
void jshell_register_cat_command(void) {
  jshell_register_command(&cmd_cat_spec);
}
