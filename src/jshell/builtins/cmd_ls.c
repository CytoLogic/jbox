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

typedef struct {
  struct arg_lit *help;
  struct arg_lit *all;
  struct arg_lit *longfmt;
  struct arg_lit *json;
  struct arg_file *paths;
  struct arg_end *end;
  void *argtable[6];
} ls_args_t;

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

static void cleanup_ls_argtable(ls_args_t *args) {
  arg_freetable(args->argtable, sizeof(args->argtable) / sizeof(args->argtable[0]));
}

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

static char get_file_type_char(mode_t mode) {
  if (S_ISDIR(mode))  return 'd';
  if (S_ISLNK(mode))  return 'l';
  if (S_ISCHR(mode))  return 'c';
  if (S_ISBLK(mode))  return 'b';
  if (S_ISFIFO(mode)) return 'p';
  if (S_ISSOCK(mode)) return 's';
  return '-';
}

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

static const char *get_file_type_string(mode_t mode) {
  if (S_ISDIR(mode))  return "directory";
  if (S_ISLNK(mode))  return "symlink";
  if (S_ISCHR(mode))  return "chardev";
  if (S_ISBLK(mode))  return "blockdev";
  if (S_ISFIFO(mode)) return "fifo";
  if (S_ISSOCK(mode)) return "socket";
  return "file";
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

const jshell_cmd_spec_t cmd_ls_spec = {
  .name = "ls",
  .summary = "list directory contents",
  .long_help = "List information about the FILEs (the current directory by default).\n"
               "Entries are sorted alphabetically.",
  .type = CMD_BUILTIN,
  .run = ls_run,
  .print_usage = ls_print_usage
};

void jshell_register_ls_command(void) {
  jshell_register_command(&cmd_ls_spec);
}
