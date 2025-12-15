#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_file *file;
  struct arg_end *end;
  void *argtable[4];
} stat_args_t;


static void build_stat_argtable(stat_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->file = arg_file1(NULL, NULL, "FILE", "file to get metadata for");
  args->end  = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->file;
  args->argtable[3] = args->end;
}


static void cleanup_stat_argtable(stat_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void stat_print_usage(FILE *out) {
  stat_args_t args;
  build_stat_argtable(&args);
  fprintf(out, "Usage: stat");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Display file metadata.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_stat_argtable(&args);
}


static const char *get_file_type_string(mode_t mode) {
  if (S_ISDIR(mode))  return "directory";
  if (S_ISLNK(mode))  return "symbolic link";
  if (S_ISCHR(mode))  return "character device";
  if (S_ISBLK(mode))  return "block device";
  if (S_ISFIFO(mode)) return "fifo";
  if (S_ISSOCK(mode)) return "socket";
  return "regular file";
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


static void format_mode_octal(mode_t mode, char *buf, size_t buf_size) {
  snprintf(buf, buf_size, "%04o", mode & 07777);
}


static int stat_run(int argc, char **argv) {
  stat_args_t args;
  build_stat_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    stat_print_usage(stdout);
    cleanup_stat_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "stat");
    fprintf(stderr, "Try 'stat --help' for more information.\n");
    cleanup_stat_argtable(&args);
    return 1;
  }

  const char *path = args.file->filename[0];
  int show_json = args.json->count > 0;

  struct stat st;
  if (lstat(path, &st) == -1) {
    if (show_json) {
      char escaped_path[512];
      escape_json_string(path, escaped_path, sizeof(escaped_path));
      printf("{\"path\": \"%s\", \"error\": \"%s\"}\n",
             escaped_path, strerror(errno));
    } else {
      fprintf(stderr, "stat: cannot stat '%s': %s\n", path, strerror(errno));
    }
    cleanup_stat_argtable(&args);
    return 1;
  }

  struct passwd *pw = getpwuid(st.st_uid);
  struct group *gr = getgrgid(st.st_gid);
  char mode_octal[8];
  format_mode_octal(st.st_mode, mode_octal, sizeof(mode_octal));

  if (show_json) {
    char escaped_path[512];
    escape_json_string(path, escaped_path, sizeof(escaped_path));

    printf("{\n");
    printf("  \"path\": \"%s\",\n", escaped_path);
    printf("  \"type\": \"%s\",\n", get_file_type_string(st.st_mode));
    printf("  \"size\": %ld,\n", (long)st.st_size);
    printf("  \"mode\": \"%s\",\n", mode_octal);
    printf("  \"uid\": %d,\n", st.st_uid);
    printf("  \"gid\": %d,\n", st.st_gid);
    printf("  \"owner\": \"%s\",\n", pw ? pw->pw_name : "unknown");
    printf("  \"group\": \"%s\",\n", gr ? gr->gr_name : "unknown");
    printf("  \"nlink\": %ld,\n", (long)st.st_nlink);
    printf("  \"inode\": %ld,\n", (long)st.st_ino);
    printf("  \"dev\": %ld,\n", (long)st.st_dev);
    printf("  \"atime\": %ld,\n", (long)st.st_atime);
    printf("  \"mtime\": %ld,\n", (long)st.st_mtime);
    printf("  \"ctime\": %ld\n", (long)st.st_ctime);
    printf("}\n");
  } else {
    char atime_buf[64], mtime_buf[64], ctime_buf[64];
    struct tm *tm;

    tm = localtime(&st.st_atime);
    strftime(atime_buf, sizeof(atime_buf), "%Y-%m-%d %H:%M:%S", tm);

    tm = localtime(&st.st_mtime);
    strftime(mtime_buf, sizeof(mtime_buf), "%Y-%m-%d %H:%M:%S", tm);

    tm = localtime(&st.st_ctime);
    strftime(ctime_buf, sizeof(ctime_buf), "%Y-%m-%d %H:%M:%S", tm);

    printf("  File: %s\n", path);
    printf("  Size: %-15ld Blocks: %-10ld IO Block: %-6ld %s\n",
           (long)st.st_size, (long)st.st_blocks, (long)st.st_blksize,
           get_file_type_string(st.st_mode));
    printf("Device: %-15lxh Inode: %-10ld Links: %ld\n",
           (unsigned long)st.st_dev, (long)st.st_ino, (long)st.st_nlink);
    printf("Access: (%s/%04o)  Uid: (%5d/%8s)   Gid: (%5d/%8s)\n",
           mode_octal, st.st_mode & 07777,
           st.st_uid, pw ? pw->pw_name : "unknown",
           st.st_gid, gr ? gr->gr_name : "unknown");
    printf("Access: %s\n", atime_buf);
    printf("Modify: %s\n", mtime_buf);
    printf("Change: %s\n", ctime_buf);
  }

  cleanup_stat_argtable(&args);
  return 0;
}


const jshell_cmd_spec_t cmd_stat_spec = {
  .name = "stat",
  .summary = "display file metadata",
  .long_help = "Display detailed information about a file including type, "
               "size, permissions, ownership, and timestamps.",
  .type = CMD_EXTERNAL,
  .run = stat_run,
  .print_usage = stat_print_usage
};


void jshell_register_stat_command(void) {
  jshell_register_command(&cmd_stat_spec);
}
