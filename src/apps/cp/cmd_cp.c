/**
 * @file cmd_cp.c
 * @brief Implementation of the cp command for copying files and directories.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "utils/jbox_signals.h"


/**
 * Arguments structure for the cp command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *recursive;
  struct arg_lit *force;
  struct arg_lit *json;
  struct arg_file *source;
  struct arg_file *dest;
  struct arg_end *end;
  void *argtable[7];
} cp_args_t;


/**
 * Initializes the argtable3 argument definitions for the cp command.
 *
 * @param args Pointer to the cp_args_t structure to initialize.
 */
static void build_cp_argtable(cp_args_t *args) {
  args->help      = arg_lit0("h", "help", "display this help and exit");
  args->recursive = arg_lit0("r", "recursive", "copy directories recursively");
  args->force     = arg_lit0("f", "force", "overwrite existing files");
  args->json      = arg_lit0(NULL, "json", "output in JSON format");
  args->source    = arg_file1(NULL, NULL, "SOURCE", "source file or directory");
  args->dest      = arg_file1(NULL, NULL, "DEST", "destination path");
  args->end       = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->recursive;
  args->argtable[2] = args->force;
  args->argtable[3] = args->json;
  args->argtable[4] = args->source;
  args->argtable[5] = args->dest;
  args->argtable[6] = args->end;
}


/**
 * Frees memory allocated by build_cp_argtable.
 *
 * @param args Pointer to the cp_args_t structure to clean up.
 */
static void cleanup_cp_argtable(cp_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the cp command.
 *
 * @param out File stream to write usage information to.
 */
static void cp_print_usage(FILE *out) {
  cp_args_t args;
  build_cp_argtable(&args);
  fprintf(out, "Usage: cp");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Copy SOURCE to DEST, or copy SOURCE into DEST directory.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_cp_argtable(&args);
}


/**
 * Escapes special characters in a string for JSON output.
 *
 * @param str      The input string to escape.
 * @param out      Buffer to write the escaped string to.
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
 * Checks if a path refers to a directory.
 *
 * @param path Path to check.
 * @return Non-zero if path is a directory, 0 otherwise.
 */
static int is_directory(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return 0;
  }
  return S_ISDIR(st.st_mode);
}


/**
 * Checks if a file or directory exists at the given path.
 *
 * @param path Path to check.
 * @return Non-zero if path exists, 0 otherwise.
 */
static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}


/**
 * Copies a single file from source to destination.
 *
 * @param src   Source file path.
 * @param dest  Destination file path.
 * @param force If non-zero, overwrite existing destination.
 * @return 0 on success, -1 on error, -2 if interrupted.
 */
static int copy_file(const char *src, const char *dest, int force) {
  if (!force && file_exists(dest)) {
    errno = EEXIST;
    return -1;
  }

  FILE *src_fp = fopen(src, "rb");
  if (!src_fp) {
    return -1;
  }

  FILE *dest_fp = fopen(dest, "wb");
  if (!dest_fp) {
    int saved_errno = errno;
    fclose(src_fp);
    errno = saved_errno;
    return -1;
  }

  char buffer[8192];
  size_t bytes_read;
  int result = 0;

  while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_fp)) > 0) {
    /* Check for interrupt */
    if (jbox_is_interrupted()) {
      fclose(src_fp);
      fclose(dest_fp);
      return -2;  /* Signal interruption */
    }

    if (fwrite(buffer, 1, bytes_read, dest_fp) != bytes_read) {
      result = -1;
      break;
    }
  }

  if (ferror(src_fp)) {
    result = -1;
  }

  struct stat src_stat;
  if (stat(src, &src_stat) == 0) {
    fchmod(fileno(dest_fp), src_stat.st_mode & 0777);
  }

  fclose(src_fp);
  fclose(dest_fp);

  return result;
}


/* Forward declaration for recursive directory copy */
static int copy_directory(const char *src, const char *dest, int force);


/**
 * Copies a file or directory entry from source to destination.
 *
 * @param src_path  Source path.
 * @param dest_path Destination path.
 * @param recursive If non-zero, copy directories recursively.
 * @param force     If non-zero, overwrite existing files.
 * @return 0 on success, -1 on error, -2 if interrupted.
 */
static int copy_entry(const char *src_path, const char *dest_path,
                      int recursive, int force) {
  struct stat st;
  if (stat(src_path, &st) != 0) {
    return -1;
  }

  if (S_ISDIR(st.st_mode)) {
    if (!recursive) {
      errno = EISDIR;
      return -1;
    }
    int result = copy_directory(src_path, dest_path, force);
    if (result == -2) return -2;  /* Propagate interruption */
    return result;
  } else {
    int result = copy_file(src_path, dest_path, force);
    if (result == -2) return -2;  /* Propagate interruption */
    return result;
  }
}


/**
 * Recursively copies a directory and its contents.
 *
 * @param src   Source directory path.
 * @param dest  Destination directory path.
 * @param force If non-zero, overwrite existing files.
 * @return 0 on success, -1 on error, -2 if interrupted.
 */
static int copy_directory(const char *src, const char *dest, int force) {
  DIR *dir = opendir(src);
  if (!dir) {
    return -1;
  }

  struct stat src_stat;
  if (stat(src, &src_stat) != 0) {
    closedir(dir);
    return -1;
  }

  if (mkdir(dest, src_stat.st_mode & 0777) != 0 && errno != EEXIST) {
    closedir(dir);
    return -1;
  }

  int result = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    /* Check for interrupt */
    if (jbox_is_interrupted()) {
      closedir(dir);
      return -2;  /* Signal interruption */
    }

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    size_t src_len = strlen(src) + strlen(entry->d_name) + 2;
    size_t dest_len = strlen(dest) + strlen(entry->d_name) + 2;

    char *src_path = malloc(src_len);
    char *dest_path = malloc(dest_len);

    if (!src_path || !dest_path) {
      free(src_path);
      free(dest_path);
      result = -1;
      break;
    }

    snprintf(src_path, src_len, "%s/%s", src, entry->d_name);
    snprintf(dest_path, dest_len, "%s/%s", dest, entry->d_name);

    int copy_result = copy_entry(src_path, dest_path, 1, force);
    if (copy_result == -2) {
      free(src_path);
      free(dest_path);
      closedir(dir);
      return -2;  /* Propagate interruption */
    }
    if (copy_result != 0) {
      result = -1;
    }

    free(src_path);
    free(dest_path);
  }

  closedir(dir);
  return result;
}


/**
 * Builds the final destination path for a copy operation.
 *
 * If dest is a directory, appends the source basename to it.
 *
 * @param src  Source path.
 * @param dest Destination path.
 * @return Newly allocated destination path string. Caller must free.
 */
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


/**
 * Main entry point for the cp command.
 *
 * Parses arguments and copies files or directories.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, non-zero on error.
 */
static int cp_run(int argc, char **argv) {
  cp_args_t args;
  build_cp_argtable(&args);

  /* Set up signal handler for clean interrupt */
  jbox_setup_sigint_handler();

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    cp_print_usage(stdout);
    cleanup_cp_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "cp");
    fprintf(stderr, "Try 'cp --help' for more information.\n");
    cleanup_cp_argtable(&args);
    return 1;
  }

  const char *source = args.source->filename[0];
  const char *dest = args.dest->filename[0];
  int recursive = args.recursive->count > 0;
  int force = args.force->count > 0;
  int show_json = args.json->count > 0;

  char *final_dest = build_dest_path(source, dest);
  if (!final_dest) {
    if (show_json) {
      printf("{\"status\": \"error\", \"message\": \"memory allocation "
             "failed\"}\n");
    } else {
      fprintf(stderr, "cp: memory allocation failed\n");
    }
    cleanup_cp_argtable(&args);
    return 1;
  }

  int result = copy_entry(source, final_dest, recursive, force);

  /* Check for interruption */
  if (result == -2) {
    free(final_dest);
    cleanup_cp_argtable(&args);
    return 130;  /* 128 + SIGINT(2) */
  }

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
      if (errno == EISDIR) {
        fprintf(stderr, "cp: -r not specified; omitting directory '%s'\n",
                source);
      } else if (errno == EEXIST) {
        fprintf(stderr, "cp: '%s' already exists (use -f to overwrite)\n",
                final_dest);
      } else {
        fprintf(stderr, "cp: cannot copy '%s' to '%s': %s\n",
                source, final_dest, strerror(errno));
      }
    }
  }

  free(final_dest);
  cleanup_cp_argtable(&args);
  return result == 0 ? 0 : 1;
}


/**
 * Command specification for the cp command.
 */
const jshell_cmd_spec_t cmd_cp_spec = {
  .name = "cp",
  .summary = "copy files and directories",
  .long_help = "Copy SOURCE to DEST, or copy SOURCE into DEST directory. "
               "With -r, copy directories recursively. "
               "With -f, overwrite existing destination files.",
  .type = CMD_EXTERNAL,
  .run = cp_run,
  .print_usage = cp_print_usage
};


/**
 * Registers the cp command with the shell command registry.
 */
void jshell_register_cp_command(void) {
  jshell_register_command(&cmd_cp_spec);
}
