/**
 * @file cmd_edit_replace.c
 * @brief Global find/replace with regex in a file command implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <ctype.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "jshell/jshell_signals.h"


/**
 * Command-line arguments for edit-replace command
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *case_insensitive;
  struct arg_lit *fixed_strings;
  struct arg_lit *json;
  struct arg_file *file;
  struct arg_str *pattern;
  struct arg_str *replacement;
  struct arg_end *end;
  void *argtable[8];
} edit_replace_args_t;


/**
 * Builds the argument table for edit-replace command.
 * @param args Pointer to argument structure to populate
 */
static void build_edit_replace_argtable(edit_replace_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->case_insensitive = arg_lit0("i", NULL, "case-insensitive matching");
  args->fixed_strings = arg_lit0(NULL, "fixed-strings",
                                  "treat pattern as literal string");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->file = arg_file1(NULL, NULL, "FILE", "file to edit");
  args->pattern = arg_str1(NULL, NULL, "PATTERN", "search pattern (regex)");
  args->replacement = arg_str1(NULL, NULL, "REPLACEMENT", "replacement text");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->case_insensitive;
  args->argtable[2] = args->fixed_strings;
  args->argtable[3] = args->json;
  args->argtable[4] = args->file;
  args->argtable[5] = args->pattern;
  args->argtable[6] = args->replacement;
  args->argtable[7] = args->end;
}


/**
 * Frees memory allocated for the argument table.
 * @param args Pointer to argument structure to clean up
 */
static void cleanup_edit_replace_argtable(edit_replace_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the edit-replace command.
 * @param out Output stream for usage information
 */
static void edit_replace_print_usage(FILE *out) {
  edit_replace_args_t args;
  build_edit_replace_argtable(&args);
  fprintf(out, "Usage: edit-replace");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Global find/replace with regex in a file.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_edit_replace_argtable(&args);
}


/**
 * Escapes special characters in a string for JSON output.
 * @param str Input string to escape
 * @param out Output buffer for escaped string
 * @param out_size Size of output buffer
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
 * Prints a JSON-formatted result message.
 * @param path File path
 * @param status Status string ("ok", "error", or "interrupted")
 * @param matches Number of matches found
 * @param replacements Number of replacements made
 * @param message Optional error message (can be NULL)
 */
static void print_json_result(const char *path, const char *status,
                              int matches, int replacements,
                              const char *message) {
  char escaped_path[512];
  escape_json_string(path, escaped_path, sizeof(escaped_path));

  if (message) {
    char escaped_msg[512];
    escape_json_string(message, escaped_msg, sizeof(escaped_msg));
    printf("{\"path\": \"%s\", \"status\": \"%s\", \"message\": \"%s\"}\n",
           escaped_path, status, escaped_msg);
  } else {
    printf("{\"path\": \"%s\", \"status\": \"%s\", "
           "\"matches\": %d, \"replacements\": %d}\n",
           escaped_path, status, matches, replacements);
  }
}


/**
 * Replaces all occurrences of a literal string with another string.
 * @param str Input string to search
 * @param old String to find
 * @param new_str Replacement string
 * @param case_insensitive Whether to perform case-insensitive matching
 * @param count Output parameter for number of replacements made
 * @return Newly allocated string with replacements, or NULL on error
 */
static char *str_replace_literal(const char *str, const char *old,
                                  const char *new_str, int case_insensitive,
                                  int *count) {
  size_t old_len = strlen(old);
  size_t new_len = strlen(new_str);
  *count = 0;

  if (old_len == 0) {
    return strdup(str);
  }

  // Count occurrences
  const char *p = str;
  int num_matches = 0;
  while (*p) {
    int match;
    if (case_insensitive) {
      match = (strncasecmp(p, old, old_len) == 0);
    } else {
      match = (strncmp(p, old, old_len) == 0);
    }
    if (match) {
      num_matches++;
      p += old_len;
    } else {
      p++;
    }
  }

  if (num_matches == 0) {
    return strdup(str);
  }

  // Allocate result
  size_t result_len = strlen(str) + num_matches * (new_len - old_len);
  char *result = malloc(result_len + 1);
  if (!result) return NULL;

  // Build result
  char *dst = result;
  p = str;
  while (*p) {
    int match;
    if (case_insensitive) {
      match = (strncasecmp(p, old, old_len) == 0);
    } else {
      match = (strncmp(p, old, old_len) == 0);
    }
    if (match) {
      memcpy(dst, new_str, new_len);
      dst += new_len;
      p += old_len;
      (*count)++;
    } else {
      *dst++ = *p++;
    }
  }
  *dst = '\0';

  return result;
}


/**
 * Replaces all matches of a compiled regex pattern with a replacement string.
 * @param str Input string to search
 * @param regex Compiled regex pattern
 * @param replacement Replacement string
 * @param count Output parameter for number of replacements made
 * @return Newly allocated string with replacements, or NULL on error
 */
static char *str_replace_regex(const char *str, regex_t *regex,
                                const char *replacement, int *count) {
  *count = 0;
  regmatch_t match;
  size_t repl_len = strlen(replacement);

  // First pass: count matches and calculate result size
  const char *p = str;
  size_t result_size = 0;
  int num_matches = 0;

  while (*p && regexec(regex, p, 1, &match, 0) == 0) {
    result_size += match.rm_so;  // Text before match
    result_size += repl_len;     // Replacement text
    p += match.rm_eo;
    num_matches++;
    if (match.rm_so == match.rm_eo) {
      // Zero-length match - advance by one to avoid infinite loop
      if (*p) {
        result_size++;
        p++;
      } else {
        break;
      }
    }
  }
  result_size += strlen(p);  // Remaining text

  if (num_matches == 0) {
    return strdup(str);
  }

  // Allocate result
  char *result = malloc(result_size + 1);
  if (!result) return NULL;

  // Second pass: build result
  char *dst = result;
  p = str;

  while (*p && regexec(regex, p, 1, &match, 0) == 0) {
    // Copy text before match
    memcpy(dst, p, match.rm_so);
    dst += match.rm_so;
    // Copy replacement
    memcpy(dst, replacement, repl_len);
    dst += repl_len;
    p += match.rm_eo;
    (*count)++;
    if (match.rm_so == match.rm_eo) {
      // Zero-length match - copy one char and advance
      if (*p) {
        *dst++ = *p++;
      } else {
        break;
      }
    }
  }
  // Copy remaining text
  strcpy(dst, p);

  return result;
}


/**
 * Dynamic buffer for storing lines of text from a file
 */
typedef struct {
  char **lines;
  size_t count;
  size_t capacity;
} line_buffer_t;


/**
 * Initializes a line buffer to empty state.
 * @param buf Pointer to line buffer to initialize
 */
static void line_buffer_init(line_buffer_t *buf) {
  buf->lines = NULL;
  buf->count = 0;
  buf->capacity = 0;
}


/**
 * Frees all memory associated with a line buffer.
 * @param buf Pointer to line buffer to free
 */
static void line_buffer_free(line_buffer_t *buf) {
  for (size_t i = 0; i < buf->count; i++) {
    free(buf->lines[i]);
  }
  free(buf->lines);
  buf->lines = NULL;
  buf->count = 0;
  buf->capacity = 0;
}


/**
 * Adds a line to the end of a line buffer.
 * @param buf Pointer to line buffer
 * @param line Line to add (will be duplicated)
 * @return 0 on success, -1 on memory allocation failure
 */
static int line_buffer_add(line_buffer_t *buf, const char *line) {
  if (buf->count >= buf->capacity) {
    size_t new_cap = buf->capacity == 0 ? 256 : buf->capacity * 2;
    char **new_lines = realloc(buf->lines, new_cap * sizeof(char *));
    if (!new_lines) return -1;
    buf->lines = new_lines;
    buf->capacity = new_cap;
  }
  buf->lines[buf->count] = strdup(line);
  if (!buf->lines[buf->count]) return -1;
  buf->count++;
  return 0;
}


/**
 * Reads all lines from a file into a line buffer.
 * @param path Path to file to read
 * @param buf Pointer to line buffer to populate
 * @return 0 on success, -1 on error, -2 if interrupted
 */
static int read_file_lines(const char *path, line_buffer_t *buf) {
  FILE *fp = fopen(path, "r");
  if (!fp) return -1;

  char *line = NULL;
  size_t line_cap = 0;
  ssize_t line_len;

  while ((line_len = getline(&line, &line_cap, fp)) != -1) {
    /* Check for interruption during file read */
    if (jshell_is_interrupted()) {
      free(line);
      fclose(fp);
      return -2;  /* Interrupted */
    }

    if (line_len > 0 && line[line_len - 1] == '\n') {
      line[line_len - 1] = '\0';
    }
    if (line_buffer_add(buf, line) != 0) {
      free(line);
      fclose(fp);
      return -1;
    }
  }

  free(line);
  fclose(fp);
  return 0;
}


/**
 * Writes all lines from a buffer to a file.
 * @param path Path to file to write
 * @param buf Pointer to line buffer containing lines to write
 * @return 0 on success, -1 on error
 */
static int write_file_lines(const char *path, line_buffer_t *buf) {
  FILE *fp = fopen(path, "w");
  if (!fp) return -1;

  for (size_t i = 0; i < buf->count; i++) {
    if (fprintf(fp, "%s\n", buf->lines[i]) < 0) {
      fclose(fp);
      return -1;
    }
  }

  fclose(fp);
  return 0;
}


/**
 * Main entry point for the edit-replace command.
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, non-zero on error, 130 if interrupted
 */
static int edit_replace_run(int argc, char **argv) {
  edit_replace_args_t args;
  build_edit_replace_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    edit_replace_print_usage(stdout);
    cleanup_edit_replace_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "edit-replace");
    fprintf(stderr, "Try 'edit-replace --help' for more information.\n");
    cleanup_edit_replace_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  int case_insensitive = args.case_insensitive->count > 0;
  int fixed_strings = args.fixed_strings->count > 0;
  const char *filepath = args.file->filename[0];
  const char *pattern = args.pattern->sval[0];
  const char *replacement = args.replacement->sval[0];

  // Compile regex if not using fixed strings
  regex_t regex;
  int regex_compiled = 0;
  if (!fixed_strings) {
    int cflags = REG_EXTENDED;
    if (case_insensitive) {
      cflags |= REG_ICASE;
    }
    int ret = regcomp(&regex, pattern, cflags);
    if (ret != 0) {
      char errbuf[256];
      regerror(ret, &regex, errbuf, sizeof(errbuf));
      if (show_json) {
        print_json_result(filepath, "error", 0, 0, errbuf);
      } else {
        fprintf(stderr, "edit-replace: invalid regex: %s\n", errbuf);
      }
      cleanup_edit_replace_argtable(&args);
      return 1;
    }
    regex_compiled = 1;
  }

  line_buffer_t lines;
  line_buffer_init(&lines);

  int read_result = read_file_lines(filepath, &lines);
  if (read_result == -2) {
    /* Interrupted by signal */
    if (show_json) {
      print_json_result(filepath, "interrupted", 0, 0, "operation interrupted");
    } else {
      fprintf(stderr, "edit-replace: interrupted\n");
    }
    line_buffer_free(&lines);
    if (regex_compiled) regfree(&regex);
    cleanup_edit_replace_argtable(&args);
    return 130;  /* 128 + SIGINT(2) */
  } else if (read_result != 0) {
    if (show_json) {
      print_json_result(filepath, "error", 0, 0, strerror(errno));
    } else {
      fprintf(stderr, "edit-replace: %s: %s\n", filepath, strerror(errno));
    }
    line_buffer_free(&lines);
    if (regex_compiled) regfree(&regex);
    cleanup_edit_replace_argtable(&args);
    return 1;
  }

  int total_matches = 0;
  int total_replacements = 0;

  for (size_t i = 0; i < lines.count; i++) {
    /* Check for interruption during replacement */
    if (jshell_is_interrupted()) {
      if (show_json) {
        print_json_result(filepath, "interrupted", total_matches,
                          total_replacements, "operation interrupted");
      } else {
        fprintf(stderr, "edit-replace: interrupted\n");
      }
      line_buffer_free(&lines);
      if (regex_compiled) regfree(&regex);
      cleanup_edit_replace_argtable(&args);
      return 130;  /* 128 + SIGINT(2) */
    }
    int count = 0;
    char *new_line;

    if (fixed_strings) {
      new_line = str_replace_literal(lines.lines[i], pattern, replacement,
                                      case_insensitive, &count);
    } else {
      new_line = str_replace_regex(lines.lines[i], &regex, replacement, &count);
    }

    if (!new_line) {
      if (show_json) {
        print_json_result(filepath, "error", 0, 0, "memory allocation failed");
      } else {
        fprintf(stderr, "edit-replace: memory allocation failed\n");
      }
      line_buffer_free(&lines);
      if (regex_compiled) regfree(&regex);
      cleanup_edit_replace_argtable(&args);
      return 1;
    }

    if (count > 0) {
      total_matches += count;
      total_replacements += count;
      free(lines.lines[i]);
      lines.lines[i] = new_line;
    } else {
      free(new_line);
    }
  }

  if (regex_compiled) {
    regfree(&regex);
  }

  if (total_replacements > 0) {
    if (write_file_lines(filepath, &lines) != 0) {
      if (show_json) {
        print_json_result(filepath, "error", 0, 0, strerror(errno));
      } else {
        fprintf(stderr, "edit-replace: failed to write %s: %s\n",
                filepath, strerror(errno));
      }
      line_buffer_free(&lines);
      cleanup_edit_replace_argtable(&args);
      return 1;
    }
  }

  if (show_json) {
    print_json_result(filepath, "ok", total_matches, total_replacements, NULL);
  }

  line_buffer_free(&lines);
  cleanup_edit_replace_argtable(&args);
  return 0;
}


/**
 * Command specification for edit-replace
 */
const jshell_cmd_spec_t cmd_edit_replace_spec = {
  .name = "edit-replace",
  .summary = "global find/replace with regex in a file",
  .long_help = "Replace all occurrences of PATTERN with REPLACEMENT in FILE. "
               "PATTERN is a regex by default; use --fixed-strings for literal.",
  .type = CMD_BUILTIN,
  .run = edit_replace_run,
  .print_usage = edit_replace_print_usage
};


/**
 * Registers the edit-replace command with the shell command registry.
 */
void jshell_register_edit_replace_command(void) {
  jshell_register_command(&cmd_edit_replace_spec);
}
