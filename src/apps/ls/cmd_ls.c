/**
 * @file cmd_ls.c
 * @brief Implementation of the ls command for listing directory contents.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"

/**
 * Arguments structure for the ls command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *all;
  struct arg_lit *longfmt;
  struct arg_lit *json;
  struct arg_file *paths;
  struct arg_end *end;
  void *argtable[6];
} ls_args_t;

/**
 * Initializes the argtable3 argument definitions for the ls command.
 *
 * @param args Pointer to the ls_args_t structure to initialize.
 */
static void build_ls_argtable(ls_args_t *args) {
  args->help    = arg_lit0("h", "help", "display this help and exit");
  args->all     = arg_lit0("a", NULL, "do not ignore entries starting with .");
  args->longfmt = arg_lit0("l", NULL, "use long listing format");
  args->json    = arg_lit0(NULL, "json", "output in JSON format");
  args->paths   = arg_filen(NULL, NULL, "PATH", 0, 100, "files or directories to list");
  args->end     = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->all;
  args->argtable[2] = args->longfmt;
  args->argtable[3] = args->json;
  args->argtable[4] = args->paths;
  args->argtable[5] = args->end;
}

/**
 * Frees memory allocated by build_ls_argtable.
 *
 * @param args Pointer to the ls_args_t structure to clean up.
 */
static void cleanup_ls_argtable(ls_args_t *args) {
  arg_freetable(args->argtable, sizeof(args->argtable) / sizeof(args->argtable[0]));
}

/**
 * Prints usage information for the ls command.
 *
 * @param out File stream to write usage information to.
 */
static void ls_print_usage(FILE *out) {
  ls_args_t args;
  build_ls_argtable(&args);
  fprintf(out, "Usage: ls");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "List directory contents.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_ls_argtable(&args);
}

/**
 * Returns a single character representing the file type.
 *
 * @param mode File mode from stat.
 * @return Character representing the file type (d, l, c, b, p, s, or -).
 */
static char get_file_type_char(mode_t mode) {
  if (S_ISDIR(mode))  return 'd';
  if (S_ISLNK(mode))  return 'l';
  if (S_ISCHR(mode))  return 'c';
  if (S_ISBLK(mode))  return 'b';
  if (S_ISFIFO(mode)) return 'p';
  if (S_ISSOCK(mode)) return 's';
  return '-';
}

/**
 * Formats file permissions into a string (e.g., "drwxr-xr-x").
 *
 * @param mode File mode from stat.
 * @param buf  Buffer of at least 11 characters to write to.
 */
static void format_permissions(mode_t mode, char *buf) {
  buf[0] = get_file_type_char(mode);
  buf[1] = (mode & S_IRUSR) ? 'r' : '-';
  buf[2] = (mode & S_IWUSR) ? 'w' : '-';
  buf[3] = (mode & S_IXUSR) ? 'x' : '-';
  buf[4] = (mode & S_IRGRP) ? 'r' : '-';
  buf[5] = (mode & S_IWGRP) ? 'w' : '-';
  buf[6] = (mode & S_IXGRP) ? 'x' : '-';
  buf[7] = (mode & S_IROTH) ? 'r' : '-';
  buf[8] = (mode & S_IWOTH) ? 'w' : '-';
  buf[9] = (mode & S_IXOTH) ? 'x' : '-';
  buf[10] = '\0';
}

/**
 * Returns a string representing the file type for JSON output.
 *
 * @param mode File mode from stat.
 * @return Static string describing the file type.
 */
static const char *get_file_type_string(mode_t mode) {
  if (S_ISDIR(mode))  return "directory";
  if (S_ISLNK(mode))  return "symlink";
  if (S_ISCHR(mode))  return "chardev";
  if (S_ISBLK(mode))  return "blockdev";
  if (S_ISFIFO(mode)) return "fifo";
  if (S_ISSOCK(mode)) return "socket";
  return "file";
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
 * Lists the contents of a directory.
 *
 * @param path        Path to the directory to list.
 * @param show_all    If non-zero, include hidden entries.
 * @param show_long   If non-zero, use long listing format.
 * @param show_json   If non-zero, output in JSON format.
 * @param first_entry Pointer to flag tracking if this is the first JSON entry.
 * @return 0 on success, 1 on error.
 */
static int list_directory(const char *path, int show_all, int show_long, int show_json, int *first_entry) {
  DIR *dir = opendir(path);
  if (!dir) {
    if (!show_json) {
      fprintf(stderr, "ls: cannot access '%s': %s\n", path, strerror(errno));
    }
    return 1;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (!show_all && entry->d_name[0] == '.') {
      continue;
    }

    char fullpath[4096];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

    struct stat st;
    if (lstat(fullpath, &st) == -1) {
      if (!show_json) {
        fprintf(stderr, "ls: cannot stat '%s': %s\n", fullpath, strerror(errno));
      }
      continue;
    }

    if (show_json) {
      char escaped_name[512];
      escape_json_string(entry->d_name, escaped_name, sizeof(escaped_name));

      if (!*first_entry) {
        printf(",\n");
      }
      *first_entry = 0;

      printf("    {\"name\": \"%s\", \"type\": \"%s\", \"size\": %ld, \"mtime\": %ld",
             escaped_name, get_file_type_string(st.st_mode),
             (long)st.st_size, (long)st.st_mtime);

      if (show_long) {
        char perms[12];
        format_permissions(st.st_mode, perms);

        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);

        printf(", \"mode\": \"%s\", \"nlink\": %ld, \"owner\": \"%s\", \"group\": \"%s\"",
               perms, (long)st.st_nlink,
               pw ? pw->pw_name : "unknown",
               gr ? gr->gr_name : "unknown");
      }
      printf("}");
    } else if (show_long) {
      char perms[12];
      format_permissions(st.st_mode, perms);

      struct passwd *pw = getpwuid(st.st_uid);
      struct group *gr = getgrgid(st.st_gid);

      char timebuf[64];
      struct tm *tm = localtime(&st.st_mtime);
      strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);

      printf("%s %3ld %-8s %-8s %8ld %s %s\n",
             perms, (long)st.st_nlink,
             pw ? pw->pw_name : "unknown",
             gr ? gr->gr_name : "unknown",
             (long)st.st_size, timebuf, entry->d_name);
    } else {
      printf("%s\n", entry->d_name);
    }
  }

  closedir(dir);
  return 0;
}

/**
 * Main entry point for the ls command.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, non-zero on error.
 */
static int ls_run(int argc, char **argv) {
  ls_args_t args;
  build_ls_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    ls_print_usage(stdout);
    cleanup_ls_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "ls");
    fprintf(stderr, "Try 'ls --help' for more information.\n");
    cleanup_ls_argtable(&args);
    return 1;
  }

  int show_all = args.all->count > 0;
  int show_long = args.longfmt->count > 0;
  int show_json = args.json->count > 0;

  int first_entry = 1;
  int result = 0;

  if (show_json) {
    printf("[\n");
  }

  if (args.paths->count == 0) {
    result = list_directory(".", show_all, show_long, show_json, &first_entry);
  } else {
    for (int i = 0; i < args.paths->count; i++) {
      struct stat st;
      if (lstat(args.paths->filename[i], &st) == -1) {
        if (!show_json) {
          fprintf(stderr, "ls: cannot access '%s': %s\n", args.paths->filename[i], strerror(errno));
        }
        result = 1;
        continue;
      }

      if (S_ISDIR(st.st_mode)) {
        if (args.paths->count > 1 && !show_json) {
          printf("%s:\n", args.paths->filename[i]);
        }
        if (list_directory(args.paths->filename[i], show_all, show_long, show_json, &first_entry) != 0) {
          result = 1;
        }
        if (args.paths->count > 1 && i < args.paths->count - 1 && !show_json) {
          printf("\n");
        }
      } else {
        if (show_json) {
          char escaped_name[512];
          escape_json_string(args.paths->filename[i], escaped_name, sizeof(escaped_name));

          if (!first_entry) {
            printf(",\n");
          }
          first_entry = 0;

          printf("    {\"name\": \"%s\", \"type\": \"%s\", \"size\": %ld, \"mtime\": %ld",
                 escaped_name, get_file_type_string(st.st_mode),
                 (long)st.st_size, (long)st.st_mtime);

          if (show_long) {
            char perms[12];
            format_permissions(st.st_mode, perms);

            struct passwd *pw = getpwuid(st.st_uid);
            struct group *gr = getgrgid(st.st_gid);

            printf(", \"mode\": \"%s\", \"nlink\": %ld, \"owner\": \"%s\", \"group\": \"%s\"",
                   perms, (long)st.st_nlink,
                   pw ? pw->pw_name : "unknown",
                   gr ? gr->gr_name : "unknown");
          }
          printf("}");
        } else if (show_long) {
          char perms[12];
          format_permissions(st.st_mode, perms);

          struct passwd *pw = getpwuid(st.st_uid);
          struct group *gr = getgrgid(st.st_gid);

          char timebuf[64];
          struct tm *tm = localtime(&st.st_mtime);
          strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);

          printf("%s %3ld %-8s %-8s %8ld %s %s\n",
                 perms, (long)st.st_nlink,
                 pw ? pw->pw_name : "unknown",
                 gr ? gr->gr_name : "unknown",
                 (long)st.st_size, timebuf, args.paths->filename[i]);
        } else {
          printf("%s\n", args.paths->filename[i]);
        }
      }
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_ls_argtable(&args);
  return result;
}

/**
 * Command specification for the ls command.
 */
const jshell_cmd_spec_t cmd_ls_spec = {
  .name = "ls",
  .summary = "list directory contents",
  .long_help = "List information about the FILEs (the current directory by default).\n"
               "Entries are sorted alphabetically.",
  .type = CMD_EXTERNAL,
  .run = ls_run,
  .print_usage = ls_print_usage
};


/**
 * Registers the ls command with the shell command registry.
 */
void jshell_register_ls_command(void) {
  jshell_register_command(&cmd_ls_spec);
}
