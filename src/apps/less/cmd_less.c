#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <ctype.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *line_numbers;
  struct arg_file *file;
  struct arg_end *end;
  void *argtable[4];
} less_args_t;


typedef struct {
  char **lines;
  size_t line_count;
  size_t top_line;
  int rows;
  int cols;
  int show_line_numbers;
  int line_num_width;
  const char *filename;
  char search_pattern[256];
  size_t *search_matches;
  size_t search_match_count;
  size_t current_match;
} less_state_t;


static struct termios orig_termios;
static volatile sig_atomic_t term_resized = 0;


static void build_less_argtable(less_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->line_numbers = arg_lit0("N", NULL, "show line numbers");
  args->file = arg_file0(NULL, NULL, "FILE", "file to view");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->line_numbers;
  args->argtable[2] = args->file;
  args->argtable[3] = args->end;
}


static void cleanup_less_argtable(less_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void less_print_usage(FILE *out) {
  less_args_t args;
  build_less_argtable(&args);
  fprintf(out, "Usage: less");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "View FILE contents with paging.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  fprintf(out, "\nNavigation:\n");
  fprintf(out, "  j, DOWN       Scroll down one line\n");
  fprintf(out, "  k, UP         Scroll up one line\n");
  fprintf(out, "  SPACE, f      Scroll down one page\n");
  fprintf(out, "  b             Scroll up one page\n");
  fprintf(out, "  g             Go to beginning\n");
  fprintf(out, "  G             Go to end\n");
  fprintf(out, "  /pattern      Search forward\n");
  fprintf(out, "  n             Next search match\n");
  fprintf(out, "  N             Previous search match\n");
  fprintf(out, "  q             Quit\n");
  cleanup_less_argtable(&args);
}


static void handle_sigwinch(int sig) {
  (void)sig;
  term_resized = 1;
}


static void disable_raw_mode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}


static int enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    return -1;
  }

  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    return -1;
  }

  return 0;
}


static void get_terminal_size(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    *rows = 24;
    *cols = 80;
  } else {
    *rows = ws.ws_row;
    *cols = ws.ws_col;
  }
}


static char **split_lines(const char *content, size_t *line_count) {
  size_t capacity = 256;
  char **lines = malloc(capacity * sizeof(char *));
  if (!lines) return NULL;

  *line_count = 0;
  const char *start = content;
  const char *end;

  while ((end = strchr(start, '\n')) != NULL) {
    if (*line_count >= capacity) {
      capacity *= 2;
      char **new_lines = realloc(lines, capacity * sizeof(char *));
      if (!new_lines) {
        for (size_t i = 0; i < *line_count; i++) free(lines[i]);
        free(lines);
        return NULL;
      }
      lines = new_lines;
    }

    size_t len = (size_t)(end - start);
    lines[*line_count] = malloc(len + 1);
    if (!lines[*line_count]) {
      for (size_t i = 0; i < *line_count; i++) free(lines[i]);
      free(lines);
      return NULL;
    }
    memcpy(lines[*line_count], start, len);
    lines[*line_count][len] = '\0';
    (*line_count)++;
    start = end + 1;
  }

  if (*start != '\0') {
    if (*line_count >= capacity) {
      capacity++;
      char **new_lines = realloc(lines, capacity * sizeof(char *));
      if (!new_lines) {
        for (size_t i = 0; i < *line_count; i++) free(lines[i]);
        free(lines);
        return NULL;
      }
      lines = new_lines;
    }
    lines[*line_count] = strdup(start);
    if (!lines[*line_count]) {
      for (size_t i = 0; i < *line_count; i++) free(lines[i]);
      free(lines);
      return NULL;
    }
    (*line_count)++;
  }

  return lines;
}


static void free_lines(char **lines, size_t line_count) {
  for (size_t i = 0; i < line_count; i++) {
    free(lines[i]);
  }
  free(lines);
}


static int count_digits(size_t n) {
  int count = 0;
  do {
    count++;
    n /= 10;
  } while (n > 0);
  return count;
}


static void clear_screen(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}


static void move_cursor(int row, int col) {
  char buf[32];
  int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
  write(STDOUT_FILENO, buf, (size_t)len);
}


static void clear_line(void) {
  write(STDOUT_FILENO, "\x1b[K", 3);
}


static void set_reverse_video(void) {
  write(STDOUT_FILENO, "\x1b[7m", 4);
}


static void reset_video(void) {
  write(STDOUT_FILENO, "\x1b[0m", 4);
}


static void draw_line(less_state_t *state, size_t line_idx, int screen_row) {
  move_cursor(screen_row, 1);
  clear_line();

  if (line_idx >= state->line_count) {
    write(STDOUT_FILENO, "~", 1);
    return;
  }

  char buf[16];
  int prefix_len = 0;

  if (state->show_line_numbers) {
    int len = snprintf(buf, sizeof(buf), "%*zu ", state->line_num_width,
                       line_idx + 1);
    write(STDOUT_FILENO, buf, (size_t)len);
    prefix_len = len;
  }

  const char *line = state->lines[line_idx];
  int max_chars = state->cols - prefix_len;
  if (max_chars < 0) max_chars = 0;

  size_t line_len = strlen(line);
  if ((int)line_len > max_chars) {
    write(STDOUT_FILENO, line, (size_t)max_chars);
  } else {
    write(STDOUT_FILENO, line, line_len);
  }
}


static void draw_status_line(less_state_t *state, const char *msg) {
  move_cursor(state->rows, 1);
  set_reverse_video();
  clear_line();

  if (msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
  } else {
    char buf[256];
    size_t end_line = state->top_line + (size_t)(state->rows - 1);
    if (end_line > state->line_count) end_line = state->line_count;

    int percent;
    if (state->line_count <= (size_t)(state->rows - 1)) {
      percent = 100;
    } else {
      percent = (int)(100 * end_line / state->line_count);
    }

    int len = snprintf(buf, sizeof(buf), " %s lines %zu-%zu/%zu (%d%%)",
                       state->filename,
                       state->top_line + 1,
                       end_line,
                       state->line_count,
                       percent);
    write(STDOUT_FILENO, buf, (size_t)len);
  }

  reset_video();
}


static void draw_screen(less_state_t *state) {
  for (int row = 1; row < state->rows; row++) {
    size_t line_idx = state->top_line + (size_t)(row - 1);
    draw_line(state, line_idx, row);
  }
  draw_status_line(state, NULL);
}


static void scroll_down(less_state_t *state, size_t lines) {
  size_t max_top = 0;
  if (state->line_count > (size_t)(state->rows - 1)) {
    max_top = state->line_count - (size_t)(state->rows - 1);
  }

  if (state->top_line + lines > max_top) {
    state->top_line = max_top;
  } else {
    state->top_line += lines;
  }
}


static void scroll_up(less_state_t *state, size_t lines) {
  if (lines > state->top_line) {
    state->top_line = 0;
  } else {
    state->top_line -= lines;
  }
}


static void goto_beginning(less_state_t *state) {
  state->top_line = 0;
}


static void goto_end(less_state_t *state) {
  if (state->line_count > (size_t)(state->rows - 1)) {
    state->top_line = state->line_count - (size_t)(state->rows - 1);
  } else {
    state->top_line = 0;
  }
}


static void search_pattern(less_state_t *state) {
  free(state->search_matches);
  state->search_matches = NULL;
  state->search_match_count = 0;
  state->current_match = 0;

  if (state->search_pattern[0] == '\0') return;

  size_t capacity = 64;
  state->search_matches = malloc(capacity * sizeof(size_t));
  if (!state->search_matches) return;

  for (size_t i = 0; i < state->line_count; i++) {
    if (strstr(state->lines[i], state->search_pattern)) {
      if (state->search_match_count >= capacity) {
        capacity *= 2;
        size_t *new_matches = realloc(state->search_matches,
                                      capacity * sizeof(size_t));
        if (!new_matches) break;
        state->search_matches = new_matches;
      }
      state->search_matches[state->search_match_count++] = i;
    }
  }
}


static void goto_next_match(less_state_t *state) {
  if (state->search_match_count == 0) return;

  for (size_t i = state->current_match; i < state->search_match_count; i++) {
    if (state->search_matches[i] > state->top_line) {
      state->current_match = i;
      state->top_line = state->search_matches[i];
      return;
    }
  }

  state->current_match = 0;
  state->top_line = state->search_matches[0];
}


static void goto_prev_match(less_state_t *state) {
  if (state->search_match_count == 0) return;

  for (size_t i = state->current_match; i > 0; i--) {
    if (state->search_matches[i - 1] < state->top_line) {
      state->current_match = i - 1;
      state->top_line = state->search_matches[i - 1];
      return;
    }
  }

  state->current_match = state->search_match_count - 1;
  state->top_line = state->search_matches[state->current_match];
}


static int read_search_input(less_state_t *state) {
  draw_status_line(state, "/");
  move_cursor(state->rows, 2);

  size_t pos = 0;
  state->search_pattern[0] = '\0';

  while (1) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) break;

    if (c == '\n' || c == '\r') {
      search_pattern(state);
      if (state->search_match_count > 0) {
        state->current_match = 0;
        for (size_t i = 0; i < state->search_match_count; i++) {
          if (state->search_matches[i] >= state->top_line) {
            state->current_match = i;
            state->top_line = state->search_matches[i];
            break;
          }
        }
      }
      return 1;
    } else if (c == 27) {
      state->search_pattern[0] = '\0';
      return 0;
    } else if (c == 127 || c == 8) {
      if (pos > 0) {
        pos--;
        state->search_pattern[pos] = '\0';
        move_cursor(state->rows, 2);
        clear_line();
        write(STDOUT_FILENO, state->search_pattern, pos);
      }
    } else if (isprint((unsigned char)c) && pos < sizeof(state->search_pattern) - 1) {
      state->search_pattern[pos++] = c;
      state->search_pattern[pos] = '\0';
      write(STDOUT_FILENO, &c, 1);
    }
  }

  return 0;
}


static int read_key(void) {
  char c;
  if (read(STDIN_FILENO, &c, 1) != 1) return -1;

  if (c == 27) {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return 27;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return 27;

    if (seq[0] == '[') {
      switch (seq[1]) {
        case 'A': return 1000;
        case 'B': return 1001;
        case 'C': return 1002;
        case 'D': return 1003;
        case '5':
          read(STDIN_FILENO, &seq[2], 1);
          return 1004;
        case '6':
          read(STDIN_FILENO, &seq[2], 1);
          return 1005;
      }
    }
    return 27;
  }

  return c;
}


static char *read_stdin_content(void) {
  size_t capacity = 4096;
  size_t size = 0;
  char *content = malloc(capacity);
  if (!content) return NULL;

  char buf[1024];
  ssize_t n;

  while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
    if (size + (size_t)n >= capacity) {
      capacity *= 2;
      char *new_content = realloc(content, capacity);
      if (!new_content) {
        free(content);
        return NULL;
      }
      content = new_content;
    }
    memcpy(content + size, buf, (size_t)n);
    size += (size_t)n;
  }

  content[size] = '\0';
  return content;
}


static char *read_file_content(const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) return NULL;

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
  return content;
}


static int less_run(int argc, char **argv) {
  less_args_t args;
  build_less_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    less_print_usage(stdout);
    cleanup_less_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "less");
    fprintf(stderr, "Try 'less --help' for more information.\n");
    cleanup_less_argtable(&args);
    return 1;
  }

  char *content = NULL;
  const char *filename = "(stdin)";

  if (args.file->count > 0) {
    filename = args.file->filename[0];
    content = read_file_content(filename);
    if (!content) {
      fprintf(stderr, "less: %s: %s\n", filename, strerror(errno));
      cleanup_less_argtable(&args);
      return 1;
    }
  } else {
    if (isatty(STDIN_FILENO)) {
      fprintf(stderr, "less: no file specified\n");
      less_print_usage(stderr);
      cleanup_less_argtable(&args);
      return 1;
    }
    content = read_stdin_content();
    if (!content) {
      fprintf(stderr, "less: failed to read stdin\n");
      cleanup_less_argtable(&args);
      return 1;
    }
  }

  size_t line_count;
  char **lines = split_lines(content, &line_count);
  free(content);

  if (!lines) {
    fprintf(stderr, "less: memory allocation failed\n");
    cleanup_less_argtable(&args);
    return 1;
  }

  if (!isatty(STDOUT_FILENO)) {
    for (size_t i = 0; i < line_count; i++) {
      if (args.line_numbers->count > 0) {
        printf("%6zu  %s\n", i + 1, lines[i]);
      } else {
        printf("%s\n", lines[i]);
      }
    }
    free_lines(lines, line_count);
    cleanup_less_argtable(&args);
    return 0;
  }

  if (enable_raw_mode() == -1) {
    fprintf(stderr, "less: failed to enable raw mode\n");
    free_lines(lines, line_count);
    cleanup_less_argtable(&args);
    return 1;
  }

  struct sigaction sa;
  sa.sa_handler = handle_sigwinch;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGWINCH, &sa, NULL);

  less_state_t state = {
    .lines = lines,
    .line_count = line_count,
    .top_line = 0,
    .show_line_numbers = args.line_numbers->count > 0,
    .filename = filename,
    .search_pattern = {0},
    .search_matches = NULL,
    .search_match_count = 0,
    .current_match = 0
  };

  get_terminal_size(&state.rows, &state.cols);
  state.line_num_width = count_digits(line_count);
  if (state.line_num_width < 4) state.line_num_width = 4;

  clear_screen();
  draw_screen(&state);

  int running = 1;
  while (running) {
    if (term_resized) {
      term_resized = 0;
      get_terminal_size(&state.rows, &state.cols);
      clear_screen();
      draw_screen(&state);
    }

    int key = read_key();

    switch (key) {
      case 'q':
      case 'Q':
        running = 0;
        break;

      case 'j':
      case 1001:
        scroll_down(&state, 1);
        draw_screen(&state);
        break;

      case 'k':
      case 1000:
        scroll_up(&state, 1);
        draw_screen(&state);
        break;

      case ' ':
      case 'f':
      case 1005:
        scroll_down(&state, (size_t)(state.rows - 2));
        draw_screen(&state);
        break;

      case 'b':
      case 1004:
        scroll_up(&state, (size_t)(state.rows - 2));
        draw_screen(&state);
        break;

      case 'g':
        goto_beginning(&state);
        draw_screen(&state);
        break;

      case 'G':
        goto_end(&state);
        draw_screen(&state);
        break;

      case '/':
        read_search_input(&state);
        draw_screen(&state);
        break;

      case 'n':
        goto_next_match(&state);
        draw_screen(&state);
        break;

      case 'N':
        goto_prev_match(&state);
        draw_screen(&state);
        break;

      case -1:
        running = 0;
        break;
    }
  }

  clear_screen();
  move_cursor(1, 1);

  free(state.search_matches);
  free_lines(lines, line_count);
  cleanup_less_argtable(&args);

  return 0;
}


const jshell_cmd_spec_t cmd_less_spec = {
  .name = "less",
  .summary = "view file contents with paging",
  .long_help = "View FILE contents with paging. "
               "Supports navigation with arrow keys, j/k, space/b, "
               "and search with /pattern.",
  .type = CMD_EXTERNAL,
  .run = less_run,
  .print_usage = less_print_usage
};


void jshell_register_less_command(void) {
  jshell_register_command(&cmd_less_spec);
}
