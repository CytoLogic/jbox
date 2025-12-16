/** @file cmd_rm.c
 *  @brief Implementation of the rm command for removing files and directories.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


/**
 * @brief Argument structure for rm command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *recursive;
  struct arg_lit *force;
  struct arg_lit *json;
  struct arg_file *files;
  struct arg_end *end;
  void *argtable[6];
} rm_args_t;


/**
 * @brief Builds the argtable3 structure for rm command arguments.
 * @param args Pointer to rm_args_t structure to populate.
 */
static void build_rm_argtable(rm_args_t *args) {
  args->help      = arg_lit0("h", "help", "display this help and exit");
  args->recursive = arg_lit0("r", "recursive", "remove directories and their "
                             "contents recursively");
  args->force     = arg_lit0("f", "force", "ignore nonexistent files, never "
                             "prompt");
  args->json      = arg_lit0(NULL, "json", "output in JSON format");
  args->files     = arg_filen(NULL, NULL, "FILE", 1, 100, "files or "
                              "directories to remove");
  args->end       = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->recursive;
  args->argtable[2] = args->force;
  args->argtable[3] = args->json;
  args->argtable[4] = args->files;
  args->argtable[5] = args->end;
}


/**
 * @brief Frees memory allocated for the rm argtable.
 * @param args Pointer to rm_args_t structure to clean up.
 */
static void cleanup_rm_argtable(rm_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * @brief Prints usage information for the rm command.
 * @param out File stream to write the usage information to.
 */
static void rm_print_usage(FILE *out) {
  rm_args_t args;
  build_rm_argtable(&args);
  fprintf(out, "Usage: rm");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Remove (unlink) the FILE(s).\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_rm_argtable(&args);
}


/**
 * @brief Escapes special characters in a string for JSON output.
 * @param str Input string to escape.
 * @param out Output buffer for escaped string.
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


static int remove_directory_recursive(const char *path);


/**
 * @brief Removes a single file or directory entry.
 * @param path Path to remove.
 * @param recursive If non-zero, recursively remove directories.
 * @param force If non-zero, ignore nonexistent files.
 * @return 0 on success, -1 on failure.
 */
static int remove_entry(const char *path, int recursive, int force) {
  struct stat st;
  if (lstat(path, &st) != 0) {
    if (force && errno == ENOENT) {
      return 0;
    }
    return -1;
  }

  if (S_ISDIR(st.st_mode)) {
    if (!recursive) {
      errno = EISDIR;
      return -1;
    }
    return remove_directory_recursive(path);
  } else {
    return unlink(path);
  }
}


/**
 * @brief Recursively removes a directory and all its contents.
 * @param path Directory path to remove.
 * @return 0 on success, -1 on failure.
 */
static int remove_directory_recursive(const char *path) {
  DIR *dir = opendir(path);
  if (!dir) {
    return -1;
  }

  int result = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    size_t path_len = strlen(path) + strlen(entry->d_name) + 2;
    char *full_path = malloc(path_len);
    if (!full_path) {
      result = -1;
      break;
    }

    snprintf(full_path, path_len, "%s/%s", path, entry->d_name);

    if (remove_entry(full_path, 1, 0) != 0) {
      result = -1;
    }

    free(full_path);
  }

  closedir(dir);

  if (result == 0) {
    result = rmdir(path);
  }

  return result;
}


/**
 * @brief Removes a file and outputs result.
 * @param path Path to remove.
 * @param recursive If non-zero, recursively remove directories.
 * @param force If non-zero, ignore nonexistent files.
 * @param show_json If non-zero, output in JSON format.
 * @param first_entry Pointer to flag tracking first JSON entry.
 * @return 0 on success, non-zero on failure.
 */
static int rm_file(const char *path, int recursive, int force, int show_json,
                   int *first_entry) {
  int result = remove_entry(path, recursive, force);

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
      if (errno == EISDIR) {
        fprintf(stderr, "rm: cannot remove '%s': Is a directory "
                        "(use -r to remove)\n", path);
      } else if (errno == ENOENT) {
        fprintf(stderr, "rm: cannot remove '%s': No such file or directory\n",
                path);
      } else {
        fprintf(stderr, "rm: cannot remove '%s': %s\n", path, strerror(errno));
      }
    }
  }

  return result;
}


/**
 * @brief Main entry point for the rm command.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on failure.
 */
static int rm_run(int argc, char **argv) {
  rm_args_t args;
  build_rm_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    rm_print_usage(stdout);
    cleanup_rm_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "rm");
    fprintf(stderr, "Try 'rm --help' for more information.\n");
    cleanup_rm_argtable(&args);
    return 1;
  }

  int recursive = args.recursive->count > 0;
  int force = args.force->count > 0;
  int show_json = args.json->count > 0;
  int first_entry = 1;
  int result = 0;

  if (show_json) {
    printf("[\n");
  }

  for (int i = 0; i < args.files->count; i++) {
    if (rm_file(args.files->filename[i], recursive, force, show_json,
                &first_entry) != 0) {
      result = 1;
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_rm_argtable(&args);
  return result;
}


/**
 * @brief Command specification for rm.
 */
const jshell_cmd_spec_t cmd_rm_spec = {
  .name = "rm",
  .summary = "remove files or directories",
  .long_help = "Remove (unlink) the FILE(s). "
               "With -r, remove directories and their contents recursively. "
               "With -f, ignore nonexistent files and never prompt.",
  .type = CMD_EXTERNAL,
  .run = rm_run,
  .print_usage = rm_print_usage
};


/**
 * @brief Registers the rm command with the shell command registry.
 */
void jshell_register_rm_command(void) {
  jshell_register_command(&cmd_rm_spec);
}
