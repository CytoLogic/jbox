/** @file cmd_rg.c
 *  @brief Search for patterns using regular expressions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "utils/jbox_signals.h"


/** Argtable structure for rg command arguments */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *line_numbers;
  struct arg_lit *ignore_case;
  struct arg_lit *word_match;
  struct arg_int *context;
  struct arg_lit *fixed_strings;
  struct arg_lit *json;
  struct arg_str *pattern;
  struct arg_file *files;
  struct arg_end *end;
  void *argtable[10];
} rg_args_t;


/**
 * Builds the argtable3 argument table for rg command.
 * @param args Pointer to rg_args_t structure to populate
 */
static void build_rg_argtable(rg_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->line_numbers = arg_lit0("n", NULL, "show line numbers");
  args->ignore_case = arg_lit0("i", NULL, "case-insensitive search");
  args->word_match = arg_lit0("w", NULL, "match whole words only");
  args->context = arg_int0("C", NULL, "N", "show N lines of context");
  args->fixed_strings = arg_lit0(NULL, "fixed-strings",
                                 "treat pattern as literal string");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->pattern = arg_str1(NULL, NULL, "PATTERN", "search pattern (regex)");
  args->files = arg_filen(NULL, NULL, "FILE", 0, 100, "files to search");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->line_numbers;
  args->argtable[2] = args->ignore_case;
  args->argtable[3] = args->word_match;
  args->argtable[4] = args->context;
  args->argtable[5] = args->fixed_strings;
  args->argtable[6] = args->json;
  args->argtable[7] = args->pattern;
  args->argtable[8] = args->files;
  args->argtable[9] = args->end;
}


/**
 * Cleans up and frees the argtable3 argument table.
 * @param args Pointer to rg_args_t structure to clean up
 */
static void cleanup_rg_argtable(rg_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the rg command.
 * @param out Output file stream
 */
static void rg_print_usage(FILE *out) {
  rg_args_t args;
  build_rg_argtable(&args);
  fprintf(out, "Usage: rg");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Search for PATTERN in each FILE.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_rg_argtable(&args);
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
 * Escapes regex metacharacters for literal string matching.
 * @param pattern Input pattern string
 * @return Newly allocated escaped pattern, or NULL on error
 */
static char *escape_regex_pattern(const char *pattern) {
  size_t len = strlen(pattern);
  char *escaped = malloc(len * 2 + 1);
  if (!escaped) return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    char c = pattern[i];
    if (strchr(".^$*+?()[{\\|", c)) {
      escaped[j++] = '\\';
    }
    escaped[j++] = c;
  }
  escaped[j] = '\0';
  return escaped;
}


/**
 * Creates a word-boundary pattern for whole-word matching.
 * @param pattern Input pattern string
 * @return Newly allocated word pattern, or NULL on error
 */
static char *create_word_pattern(const char *pattern) {
  size_t len = strlen(pattern);
  char *word_pat = malloc(len + 20);
  if (!word_pat) return NULL;
  snprintf(word_pat, len + 20, "\\b%s\\b", pattern);
  return word_pat;
}


/** Dynamic buffer for storing file lines */
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
 * Adds a line to the line buffer.
 * @param buf Pointer to line buffer
 * @param line Line to add (will be duplicated)
 * @return 0 on success, -1 on error
 */
static int line_buffer_add(line_buffer_t *buf, const char *line) {
  if (buf->count >= buf->capacity) {
    size_t new_cap = buf->capacity == 0 ? 1024 : buf->capacity * 2;
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
 * @param path File path to read
 * @param buf Line buffer to populate
 * @return 0 on success, -1 on error, -2 on interrupt
 */
static int read_file_lines(const char *path, line_buffer_t *buf) {
  FILE *fp = fopen(path, "r");
  if (!fp) return -1;

  char *line = NULL;
  size_t line_cap = 0;
  ssize_t line_len;

  while ((line_len = getline(&line, &line_cap, fp)) != -1) {
    /* Check for interrupt */
    if (jbox_is_interrupted()) {
      free(line);
      fclose(fp);
      return -2;  /* Signal interruption */
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
 * Finds the column position of the first regex match in a line.
 * @param line Line to search
 * @param regex Compiled regex pattern
 * @return 1-based column position, or 1 if not found
 */
static int find_column(const char *line, regex_t *regex) {
  regmatch_t match;
  if (regexec(regex, line, 1, &match, 0) == 0) {
    return (int)(match.rm_so + 1);
  }
  return 1;
}


/** Result of a pattern match */
typedef struct {
  const char *file;
  int line;
  int column;
  const char *text;
} match_result_t;


/**
 * Prints a match result in JSON format.
 * @param match Match result to print
 * @param first Pointer to flag indicating if this is the first JSON entry
 */
static void print_match_json(const match_result_t *match, int *first) {
  if (!*first) {
    printf(",\n");
  }
  *first = 0;

  char escaped_file[512];
  escape_json_string(match->file, escaped_file, sizeof(escaped_file));

  size_t text_len = strlen(match->text);
  char *escaped_text = malloc(text_len * 2 + 1);
  if (!escaped_text) {
    printf("{\"file\": \"%s\", \"line\": %d, \"column\": %d, "
           "\"text\": \"<error>\"}",
           escaped_file, match->line, match->column);
    return;
  }
  escape_json_string(match->text, escaped_text, text_len * 2 + 1);

  printf("{\"file\": \"%s\", \"line\": %d, \"column\": %d, \"text\": \"%s\"}",
         escaped_file, match->line, match->column, escaped_text);

  free(escaped_text);
}


/**
 * Prints a match result in text format.
 * @param match Match result to print
 * @param show_filename Whether to show filename
 * @param show_line_numbers Whether to show line numbers
 */
static void print_match_text(const match_result_t *match, int show_filename,
                             int show_line_numbers) {
  if (show_filename && show_line_numbers) {
    printf("%s:%d:%s\n", match->file, match->line, match->text);
  } else if (show_filename) {
    printf("%s:%s\n", match->file, match->text);
  } else if (show_line_numbers) {
    printf("%d:%s\n", match->line, match->text);
  } else {
    printf("%s\n", match->text);
  }
}


/**
 * Prints a context line (before or after a match).
 * @param file Filename
 * @param line_num Line number
 * @param text Line text
 * @param show_filename Whether to show filename
 * @param show_line_numbers Whether to show line numbers
 * @param separator Separator character (':' for match, '-' for context)
 */
static void print_context_line(const char *file, int line_num,
                               const char *text, int show_filename,
                               int show_line_numbers, char separator) {
  if (show_filename && show_line_numbers) {
    printf("%s%c%d%c%s\n", file, separator, line_num, separator, text);
  } else if (show_filename) {
    printf("%s%c%s\n", file, separator, text);
  } else if (show_line_numbers) {
    printf("%d%c%s\n", line_num, separator, text);
  } else {
    printf("%s\n", text);
  }
}


/**
 * Searches a file for pattern matches.
 * @param path File path to search
 * @param regex Compiled regex pattern
 * @param show_json Whether to output JSON
 * @param show_line_numbers Whether to show line numbers
 * @param show_filename Whether to show filename
 * @param context_lines Number of context lines to show
 * @param first_json_entry Pointer to first JSON entry flag
 * @param found_any Pointer to flag indicating if any matches found
 * @return 0 on success, 1 on error, -2 on interrupt
 */
static int search_file(const char *path, regex_t *regex, int show_json,
                       int show_line_numbers, int show_filename,
                       int context_lines, int *first_json_entry,
                       int *found_any) {
  line_buffer_t lines;
  line_buffer_init(&lines);

  int read_result = read_file_lines(path, &lines);
  if (read_result == -2) {
    return -2;  /* Propagate interruption */
  }
  if (read_result != 0) {
    if (show_json) {
      if (!*first_json_entry) printf(",\n");
      *first_json_entry = 0;
      char escaped_path[512];
      escape_json_string(path, escaped_path, sizeof(escaped_path));
      printf("{\"file\": \"%s\", \"error\": \"%s\"}",
             escaped_path, strerror(errno));
    } else {
      fprintf(stderr, "rg: %s: %s\n", path, strerror(errno));
    }
    return 1;
  }

  int *matched = calloc(lines.count, sizeof(int));
  if (!matched) {
    line_buffer_free(&lines);
    return 1;
  }

  for (size_t i = 0; i < lines.count; i++) {
    if (regexec(regex, lines.lines[i], 0, NULL, 0) == 0) {
      matched[i] = 1;
      *found_any = 1;
    }
  }

  int last_printed = -1;
  int need_separator = 0;

  for (size_t i = 0; i < lines.count; i++) {
    if (!matched[i]) continue;

    if (show_json) {
      match_result_t match = {
        .file = path,
        .line = (int)(i + 1),
        .column = find_column(lines.lines[i], regex),
        .text = lines.lines[i]
      };
      print_match_json(&match, first_json_entry);
    } else {
      if (context_lines > 0) {
        int ctx_start = (int)i - context_lines;
        if (ctx_start < 0) ctx_start = 0;
        int ctx_end = (int)i + context_lines;
        if (ctx_end >= (int)lines.count) ctx_end = (int)lines.count - 1;

        if (need_separator && ctx_start > last_printed + 1) {
          printf("--\n");
        }

        for (int j = ctx_start; j <= ctx_end; j++) {
          if (j <= last_printed) continue;

          if (matched[j]) {
            print_context_line(path, j + 1, lines.lines[j], show_filename,
                               show_line_numbers, ':');
          } else {
            print_context_line(path, j + 1, lines.lines[j], show_filename,
                               show_line_numbers, '-');
          }
          last_printed = j;
        }
        need_separator = 1;
      } else {
        match_result_t match = {
          .file = path,
          .line = (int)(i + 1),
          .column = find_column(lines.lines[i], regex),
          .text = lines.lines[i]
        };
        print_match_text(&match, show_filename, show_line_numbers);
      }
    }
  }

  free(matched);
  line_buffer_free(&lines);
  return 0;
}


/**
 * Searches standard input for pattern matches.
 * @param regex Compiled regex pattern
 * @param show_json Whether to output JSON
 * @param show_line_numbers Whether to show line numbers
 * @param context_lines Number of context lines to show
 * @param first_json_entry Pointer to first JSON entry flag
 * @param found_any Pointer to flag indicating if any matches found
 * @return 0 on success, 1 on error, -2 on interrupt
 */
static int search_stdin(regex_t *regex, int show_json, int show_line_numbers,
                        int context_lines, int *first_json_entry,
                        int *found_any) {
  line_buffer_t lines;
  line_buffer_init(&lines);

  char *line = NULL;
  size_t line_cap = 0;
  ssize_t line_len;

  while ((line_len = getline(&line, &line_cap, stdin)) != -1) {
    /* Check for interrupt */
    if (jbox_is_interrupted()) {
      free(line);
      line_buffer_free(&lines);
      return -2;  /* Signal interruption */
    }

    if (line_len > 0 && line[line_len - 1] == '\n') {
      line[line_len - 1] = '\0';
    }
    if (line_buffer_add(&lines, line) != 0) {
      free(line);
      line_buffer_free(&lines);
      return 1;
    }
  }
  free(line);

  int *matched = calloc(lines.count, sizeof(int));
  if (!matched) {
    line_buffer_free(&lines);
    return 1;
  }

  for (size_t i = 0; i < lines.count; i++) {
    if (regexec(regex, lines.lines[i], 0, NULL, 0) == 0) {
      matched[i] = 1;
      *found_any = 1;
    }
  }

  int last_printed = -1;
  int need_separator = 0;

  for (size_t i = 0; i < lines.count; i++) {
    if (!matched[i]) continue;

    if (show_json) {
      match_result_t match = {
        .file = "(stdin)",
        .line = (int)(i + 1),
        .column = find_column(lines.lines[i], regex),
        .text = lines.lines[i]
      };
      print_match_json(&match, first_json_entry);
    } else {
      if (context_lines > 0) {
        int ctx_start = (int)i - context_lines;
        if (ctx_start < 0) ctx_start = 0;
        int ctx_end = (int)i + context_lines;
        if (ctx_end >= (int)lines.count) ctx_end = (int)lines.count - 1;

        if (need_separator && ctx_start > last_printed + 1) {
          printf("--\n");
        }

        for (int j = ctx_start; j <= ctx_end; j++) {
          if (j <= last_printed) continue;

          if (matched[j]) {
            print_context_line("(stdin)", j + 1, lines.lines[j], 0,
                               show_line_numbers, ':');
          } else {
            print_context_line("(stdin)", j + 1, lines.lines[j], 0,
                               show_line_numbers, '-');
          }
          last_printed = j;
        }
        need_separator = 1;
      } else {
        match_result_t match = {
          .file = "(stdin)",
          .line = (int)(i + 1),
          .column = find_column(lines.lines[i], regex),
          .text = lines.lines[i]
        };
        print_match_text(&match, 0, show_line_numbers);
      }
    }
  }

  free(matched);
  line_buffer_free(&lines);
  return 0;
}


/**
 * Main entry point for the rg command.
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status (0 on success, 1 on error or no matches)
 */
static int rg_run(int argc, char **argv) {
  rg_args_t args;
  build_rg_argtable(&args);

  /* Set up signal handler for clean interrupt */
  jbox_setup_sigint_handler();

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    rg_print_usage(stdout);
    cleanup_rg_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "rg");
    fprintf(stderr, "Try 'rg --help' for more information.\n");
    cleanup_rg_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  int show_line_numbers = args.line_numbers->count > 0;
  int ignore_case = args.ignore_case->count > 0;
  int word_match = args.word_match->count > 0;
  int fixed_strings = args.fixed_strings->count > 0;
  int context_lines = args.context->count > 0 ? args.context->ival[0] : 0;
  const char *pattern = args.pattern->sval[0];
  int file_count = args.files->count;

  char *search_pattern = NULL;

  if (fixed_strings) {
    search_pattern = escape_regex_pattern(pattern);
    if (!search_pattern) {
      fprintf(stderr, "rg: memory allocation failed\n");
      cleanup_rg_argtable(&args);
      return 1;
    }
  } else {
    search_pattern = strdup(pattern);
    if (!search_pattern) {
      fprintf(stderr, "rg: memory allocation failed\n");
      cleanup_rg_argtable(&args);
      return 1;
    }
  }

  if (word_match) {
    char *word_pat = create_word_pattern(search_pattern);
    free(search_pattern);
    if (!word_pat) {
      fprintf(stderr, "rg: memory allocation failed\n");
      cleanup_rg_argtable(&args);
      return 1;
    }
    search_pattern = word_pat;
  }

  int cflags = REG_EXTENDED | REG_NEWLINE;
  if (ignore_case) {
    cflags |= REG_ICASE;
  }

  regex_t regex;
  int regex_err = regcomp(&regex, search_pattern, cflags);
  free(search_pattern);

  if (regex_err != 0) {
    char err_buf[256];
    regerror(regex_err, &regex, err_buf, sizeof(err_buf));
    fprintf(stderr, "rg: invalid pattern: %s\n", err_buf);
    cleanup_rg_argtable(&args);
    return 1;
  }

  int first_json_entry = 1;
  int result = 0;
  int found_any = 0;
  int show_filename = file_count > 1;

  if (show_json) {
    printf("[\n");
  }

  if (file_count == 0) {
    int search_result = search_stdin(&regex, show_json, show_line_numbers,
                                     context_lines, &first_json_entry,
                                     &found_any);
    if (search_result == -2) {
      if (show_json) printf("\n]\n");
      regfree(&regex);
      cleanup_rg_argtable(&args);
      return 130;  /* 128 + SIGINT(2) */
    }
    if (search_result != 0) {
      result = 1;
    }
  } else {
    for (int i = 0; i < file_count; i++) {
      /* Check for interrupt between files */
      if (jbox_is_interrupted()) {
        if (show_json) printf("\n]\n");
        regfree(&regex);
        cleanup_rg_argtable(&args);
        return 130;  /* 128 + SIGINT(2) */
      }

      int search_result = search_file(args.files->filename[i], &regex,
                                      show_json, show_line_numbers,
                                      show_filename, context_lines,
                                      &first_json_entry, &found_any);
      if (search_result == -2) {
        if (show_json) printf("\n]\n");
        regfree(&regex);
        cleanup_rg_argtable(&args);
        return 130;  /* 128 + SIGINT(2) */
      }
      if (search_result != 0) {
        result = 1;
      }
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  regfree(&regex);
  cleanup_rg_argtable(&args);

  if (result != 0) return result;
  return found_any ? 0 : 1;
}


/** Command specification for rg */
const jshell_cmd_spec_t cmd_rg_spec = {
  .name = "rg",
  .summary = "search for patterns using regular expressions",
  .long_help = "Search for PATTERN in each FILE or standard input. "
               "PATTERN is a POSIX extended regular expression by default. "
               "Use --fixed-strings to treat PATTERN as a literal string.",
  .type = CMD_EXTERNAL,
  .run = rg_run,
  .print_usage = rg_print_usage
};


/**
 * Registers the rg command with the shell command registry.
 */
void jshell_register_rg_command(void) {
  jshell_register_command(&cmd_rg_spec);
}
