/** @file cmd_vi.c
 *  @brief Vi-like text editor implementation
 */

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


/** Vi editor modes */
typedef enum {
  MODE_NORMAL,
  MODE_INSERT,
  MODE_COMMAND,
  MODE_SEARCH
} vi_mode_t;


/** Argtable structure for vi command arguments */
typedef struct {
  struct arg_lit *help;
  struct arg_file *file;
  struct arg_end *end;
  void *argtable[3];
} vi_args_t;


/** Single line in the editor buffer */
typedef struct {
  char *text;
  size_t len;
  size_t capacity;
} vi_line_t;


/** Complete editor state */
typedef struct {
  vi_line_t *lines;
  size_t line_count;
  size_t line_capacity;
  size_t cursor_row;
  size_t cursor_col;
  size_t top_line;
  int rows;
  int cols;
  vi_mode_t mode;
  char *filename;
  int modified;
  char status_msg[256];
  char command_buf[256];
  size_t command_len;
  char yank_buf[4096];
  int yank_is_line;
} vi_state_t;


static struct termios orig_termios;
static volatile sig_atomic_t term_resized = 0;
static volatile sig_atomic_t term_suspended = 0;
static volatile sig_atomic_t term_terminated = 0;


/**
 * Builds the argtable3 argument table for vi command.
 * @param args Pointer to vi_args_t structure to populate
 */
static void build_vi_argtable(vi_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->file = arg_file0(NULL, NULL, "FILE", "file to edit");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->file;
  args->argtable[2] = args->end;
}


/**
 * Cleans up and frees the argtable3 argument table.
 * @param args Pointer to vi_args_t structure to clean up
 */
static void cleanup_vi_argtable(vi_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the vi command.
 * @param out Output file stream
 */
static void vi_print_usage(FILE *out) {
  vi_args_t args;
  build_vi_argtable(&args);
  fprintf(out, "Usage: vi");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Edit FILE with vi-like interface.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  fprintf(out, "\nNormal mode commands:\n");
  fprintf(out, "  h, LEFT       Move cursor left\n");
  fprintf(out, "  j, DOWN       Move cursor down\n");
  fprintf(out, "  k, UP         Move cursor up\n");
  fprintf(out, "  l, RIGHT      Move cursor right\n");
  fprintf(out, "  0             Move to beginning of line\n");
  fprintf(out, "  $             Move to end of line\n");
  fprintf(out, "  gg            Go to first line\n");
  fprintf(out, "  G             Go to last line\n");
  fprintf(out, "  w             Move forward by word\n");
  fprintf(out, "  b             Move backward by word\n");
  fprintf(out, "  i             Enter insert mode\n");
  fprintf(out, "  a             Enter insert mode after cursor\n");
  fprintf(out, "  o             Open line below\n");
  fprintf(out, "  O             Open line above\n");
  fprintf(out, "  x             Delete character under cursor\n");
  fprintf(out, "  dd            Delete line\n");
  fprintf(out, "  yy            Yank (copy) line\n");
  fprintf(out, "  p             Paste after cursor/line\n");
  fprintf(out, "  P             Paste before cursor/line\n");
  fprintf(out, "  u             Undo (limited)\n");
  fprintf(out, "  :             Enter command mode\n");
  fprintf(out, "  /             Search forward\n");
  fprintf(out, "\nCommand mode:\n");
  fprintf(out, "  :w            Write file\n");
  fprintf(out, "  :q            Quit (fails if modified)\n");
  fprintf(out, "  :q!           Quit without saving\n");
  fprintf(out, "  :wq           Write and quit\n");
  fprintf(out, "  :e FILE       Edit FILE\n");
  cleanup_vi_argtable(&args);
}


/**
 * Signal handler for terminal window resize (SIGWINCH).
 * @param sig Signal number
 */
static void handle_sigwinch(int sig) {
  (void)sig;
  term_resized = 1;
}


/**
 * Signal handler for terminal suspend (SIGTSTP).
 * @param sig Signal number
 */
static void handle_sigtstp(int sig) {
  (void)sig;
  term_suspended = 1;
}


/**
 * Signal handler for terminal continue (SIGCONT).
 * @param sig Signal number
 */
static void handle_sigcont(int sig) {
  (void)sig;
  /* Terminal state will be restored in main loop */
}


/**
 * Signal handler for terminal termination (SIGTERM).
 * @param sig Signal number
 */
static void handle_sigterm(int sig) {
  (void)sig;
  term_terminated = 1;
}


/**
 * Disables raw terminal mode and restores original settings.
 */
static void disable_raw_mode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}


/**
 * Enables raw terminal mode for vi-style editing.
 * @return 0 on success, -1 on error
 */
static int enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    return -1;
  }

  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    return -1;
  }

  return 0;
}


/**
 * Gets the current terminal size.
 * @param rows Pointer to store number of rows
 * @param cols Pointer to store number of columns
 */
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


/**
 * Initializes a line structure.
 * @param line Pointer to line to initialize
 */
static void vi_line_init(vi_line_t *line) {
  line->capacity = 64;
  line->text = malloc(line->capacity);
  line->text[0] = '\0';
  line->len = 0;
}


/**
 * Frees memory associated with a line.
 * @param line Pointer to line to free
 */
static void vi_line_free(vi_line_t *line) {
  free(line->text);
  line->text = NULL;
  line->len = 0;
  line->capacity = 0;
}


/**
 * Ensures a line has sufficient capacity for specified size.
 * @param line Pointer to line
 * @param needed Required capacity
 */
static void vi_line_ensure_capacity(vi_line_t *line, size_t needed) {
  if (needed >= line->capacity) {
    size_t new_cap = line->capacity * 2;
    if (new_cap < needed + 1) new_cap = needed + 1;
    char *new_text = realloc(line->text, new_cap);
    if (new_text) {
      line->text = new_text;
      line->capacity = new_cap;
    }
  }
}


/**
 * Inserts a character at specified position in a line.
 * @param line Pointer to line
 * @param pos Position to insert at
 * @param c Character to insert
 */
static void vi_line_insert_char(vi_line_t *line, size_t pos, char c) {
  vi_line_ensure_capacity(line, line->len + 1);
  if (pos > line->len) pos = line->len;
  memmove(line->text + pos + 1, line->text + pos, line->len - pos + 1);
  line->text[pos] = c;
  line->len++;
}


/**
 * Deletes a character at specified position in a line.
 * @param line Pointer to line
 * @param pos Position to delete from
 */
static void vi_line_delete_char(vi_line_t *line, size_t pos) {
  if (pos >= line->len) return;
  memmove(line->text + pos, line->text + pos + 1, line->len - pos);
  line->len--;
}


/**
 * Sets the content of a line.
 * @param line Pointer to line
 * @param text Text to set
 * @param len Length of text
 */
static void vi_line_set(vi_line_t *line, const char *text, size_t len) {
  vi_line_ensure_capacity(line, len);
  memcpy(line->text, text, len);
  line->text[len] = '\0';
  line->len = len;
}


/**
 * Initializes the editor state.
 * @param state Pointer to state structure to initialize
 */
static void vi_state_init(vi_state_t *state) {
  state->line_capacity = 256;
  state->lines = malloc(state->line_capacity * sizeof(vi_line_t));
  state->line_count = 1;
  vi_line_init(&state->lines[0]);
  state->cursor_row = 0;
  state->cursor_col = 0;
  state->top_line = 0;
  state->mode = MODE_NORMAL;
  state->filename = NULL;
  state->modified = 0;
  state->status_msg[0] = '\0';
  state->command_buf[0] = '\0';
  state->command_len = 0;
  state->yank_buf[0] = '\0';
  state->yank_is_line = 0;
  get_terminal_size(&state->rows, &state->cols);
}


/**
 * Frees all memory associated with editor state.
 * @param state Pointer to state structure to free
 */
static void vi_state_free(vi_state_t *state) {
  for (size_t i = 0; i < state->line_count; i++) {
    vi_line_free(&state->lines[i]);
  }
  free(state->lines);
  free(state->filename);
}


/**
 * Ensures the editor has capacity for specified number of lines.
 * @param state Pointer to state structure
 * @param needed Required line capacity
 */
static void vi_ensure_line_capacity(vi_state_t *state, size_t needed) {
  if (needed >= state->line_capacity) {
    size_t new_cap = state->line_capacity * 2;
    if (new_cap < needed + 1) new_cap = needed + 1;
    vi_line_t *new_lines = realloc(state->lines, new_cap * sizeof(vi_line_t));
    if (new_lines) {
      state->lines = new_lines;
      state->line_capacity = new_cap;
    }
  }
}


/**
 * Inserts a new line at specified position.
 * @param state Pointer to state structure
 * @param pos Position to insert new line
 */
static void vi_insert_line(vi_state_t *state, size_t pos) {
  vi_ensure_line_capacity(state, state->line_count + 1);
  if (pos > state->line_count) pos = state->line_count;
  memmove(&state->lines[pos + 1], &state->lines[pos],
          (state->line_count - pos) * sizeof(vi_line_t));
  vi_line_init(&state->lines[pos]);
  state->line_count++;
  state->modified = 1;
}


/**
 * Deletes a line at specified position.
 * @param state Pointer to state structure
 * @param pos Position of line to delete
 */
static void vi_delete_line(vi_state_t *state, size_t pos) {
  if (state->line_count <= 1) {
    vi_line_set(&state->lines[0], "", 0);
    state->modified = 1;
    return;
  }
  if (pos >= state->line_count) return;
  vi_line_free(&state->lines[pos]);
  memmove(&state->lines[pos], &state->lines[pos + 1],
          (state->line_count - pos - 1) * sizeof(vi_line_t));
  state->line_count--;
  state->modified = 1;
}


/**
 * Loads a file into the editor.
 * @param state Pointer to state structure
 * @param path File path to load
 * @return 0 on success, -1 on error
 */
static int vi_load_file(vi_state_t *state, const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    if (errno == ENOENT) {
      free(state->filename);
      state->filename = strdup(path);
      snprintf(state->status_msg, sizeof(state->status_msg),
               "\"%s\" [New File]", path);
      return 0;
    }
    return -1;
  }

  for (size_t i = 0; i < state->line_count; i++) {
    vi_line_free(&state->lines[i]);
  }
  state->line_count = 0;

  char buf[4096];
  while (fgets(buf, sizeof(buf), fp)) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
      len--;
    }
    vi_ensure_line_capacity(state, state->line_count + 1);
    vi_line_init(&state->lines[state->line_count]);
    vi_line_set(&state->lines[state->line_count], buf, len);
    state->line_count++;
  }

  fclose(fp);

  if (state->line_count == 0) {
    vi_line_init(&state->lines[0]);
    state->line_count = 1;
  }

  free(state->filename);
  state->filename = strdup(path);
  state->cursor_row = 0;
  state->cursor_col = 0;
  state->top_line = 0;
  state->modified = 0;
  snprintf(state->status_msg, sizeof(state->status_msg),
           "\"%s\" %zuL", path, state->line_count);

  return 0;
}


/**
 * Saves the current file.
 * @param state Pointer to state structure
 * @return 0 on success, -1 on error
 */
static int vi_save_file(vi_state_t *state) {
  if (!state->filename) {
    snprintf(state->status_msg, sizeof(state->status_msg),
             "No file name");
    return -1;
  }

  FILE *fp = fopen(state->filename, "w");
  if (!fp) {
    snprintf(state->status_msg, sizeof(state->status_msg),
             "Cannot write: %s", strerror(errno));
    return -1;
  }

  for (size_t i = 0; i < state->line_count; i++) {
    fprintf(fp, "%s\n", state->lines[i].text);
  }

  fclose(fp);
  state->modified = 0;
  snprintf(state->status_msg, sizeof(state->status_msg),
           "\"%s\" %zuL written", state->filename, state->line_count);
  return 0;
}


/**
 * Clamps the cursor position to valid bounds.
 * @param state Pointer to state structure
 */
static void vi_clamp_cursor(vi_state_t *state) {
  if (state->cursor_row >= state->line_count) {
    state->cursor_row = state->line_count > 0 ? state->line_count - 1 : 0;
  }
  vi_line_t *line = &state->lines[state->cursor_row];
  size_t max_col = line->len > 0 ? line->len - 1 : 0;
  if (state->mode == MODE_INSERT) {
    max_col = line->len;
  }
  if (state->cursor_col > max_col) {
    state->cursor_col = max_col;
  }
}


/**
 * Scrolls the view to ensure cursor is visible.
 * @param state Pointer to state structure
 */
static void vi_scroll_to_cursor(vi_state_t *state) {
  int text_rows = state->rows - 2;
  if (text_rows < 1) text_rows = 1;

  if (state->cursor_row < state->top_line) {
    state->top_line = state->cursor_row;
  } else if (state->cursor_row >= state->top_line + (size_t)text_rows) {
    state->top_line = state->cursor_row - (size_t)text_rows + 1;
  }
}


/**
 * Clears the terminal screen.
 */
static void clear_screen(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}


/**
 * Hides the terminal cursor.
 */
static void hide_cursor(void) {
  write(STDOUT_FILENO, "\x1b[?25l", 6);
}


/**
 * Shows the terminal cursor.
 */
static void show_cursor(void) {
  write(STDOUT_FILENO, "\x1b[?25h", 6);
}


/**
 * Suspends the editor (Ctrl+Z).
 */
static void vi_suspend(void) {
  /* Restore terminal to normal mode */
  disable_raw_mode();
  show_cursor();

  /* Send SIGSTOP to ourselves */
  raise(SIGSTOP);

  /* When we resume (after SIGCONT), restore raw mode */
  enable_raw_mode();

  /* Force terminal resize check and redraw */
  term_resized = 1;
}


/**
 * Performs an emergency save to a swap file.
 * @param state Pointer to state structure
 */
static void vi_emergency_save(vi_state_t *state) {
  if (state->modified && state->filename) {
    char backup[512];
    snprintf(backup, sizeof(backup), "%s.swp", state->filename);
    FILE *fp = fopen(backup, "w");
    if (fp) {
      for (size_t i = 0; i < state->line_count; i++) {
        fprintf(fp, "%s\n", state->lines[i].text);
      }
      fclose(fp);
    }
  }
}


/**
 * Moves the terminal cursor to specified position.
 * @param row Row position (1-based)
 * @param col Column position (1-based)
 */
static void move_cursor(int row, int col) {
  char buf[32];
  int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
  write(STDOUT_FILENO, buf, (size_t)len);
}


/**
 * Clears the current terminal line.
 */
static void clear_line(void) {
  write(STDOUT_FILENO, "\x1b[K", 3);
}


/**
 * Sets terminal to reverse video mode.
 */
static void set_reverse_video(void) {
  write(STDOUT_FILENO, "\x1b[7m", 4);
}


/**
 * Resets terminal video attributes to normal.
 */
static void reset_video(void) {
  write(STDOUT_FILENO, "\x1b[0m", 4);
}


/**
 * Draws a single row of the editor.
 * @param state Pointer to state structure
 * @param screen_row Screen row position
 * @param file_row File line number to display
 */
static void vi_draw_row(vi_state_t *state, int screen_row, size_t file_row) {
  move_cursor(screen_row, 1);
  clear_line();

  if (file_row >= state->line_count) {
    write(STDOUT_FILENO, "~", 1);
    return;
  }

  vi_line_t *line = &state->lines[file_row];
  size_t display_len = line->len;
  if ((int)display_len > state->cols) {
    display_len = (size_t)state->cols;
  }

  if (display_len > 0) {
    write(STDOUT_FILENO, line->text, display_len);
  }
}


/**
 * Draws the status bar at bottom of screen.
 * @param state Pointer to state structure
 */
static void vi_draw_status_bar(vi_state_t *state) {
  move_cursor(state->rows - 1, 1);
  set_reverse_video();
  clear_line();

  char left[128];
  char right[64];
  const char *mode_str;
  switch (state->mode) {
    case MODE_INSERT:
      mode_str = "-- INSERT --";
      break;
    case MODE_COMMAND:
    case MODE_SEARCH:
      mode_str = "";
      break;
    default:
      mode_str = "";
      break;
  }

  int left_len = snprintf(left, sizeof(left), " %s %s%s",
                          state->filename ? state->filename : "[No Name]",
                          state->modified ? "[+] " : "",
                          mode_str);
  int right_len = snprintf(right, sizeof(right), "%zu/%zu ",
                           state->cursor_row + 1, state->line_count);

  write(STDOUT_FILENO, left, (size_t)left_len);

  int padding = state->cols - left_len - right_len;
  for (int i = 0; i < padding; i++) {
    write(STDOUT_FILENO, " ", 1);
  }
  write(STDOUT_FILENO, right, (size_t)right_len);

  reset_video();
}


/**
 * Draws the message bar at bottom of screen.
 * @param state Pointer to state structure
 */
static void vi_draw_message_bar(vi_state_t *state) {
  move_cursor(state->rows, 1);
  clear_line();

  if (state->mode == MODE_COMMAND) {
    write(STDOUT_FILENO, ":", 1);
    write(STDOUT_FILENO, state->command_buf, state->command_len);
  } else if (state->mode == MODE_SEARCH) {
    write(STDOUT_FILENO, "/", 1);
    write(STDOUT_FILENO, state->command_buf, state->command_len);
  } else if (state->status_msg[0] != '\0') {
    size_t msg_len = strlen(state->status_msg);
    if ((int)msg_len > state->cols) msg_len = (size_t)state->cols;
    write(STDOUT_FILENO, state->status_msg, msg_len);
  }
}


/**
 * Draws the entire editor screen.
 * @param state Pointer to state structure
 */
static void vi_draw_screen(vi_state_t *state) {
  hide_cursor();

  int text_rows = state->rows - 2;
  for (int i = 0; i < text_rows; i++) {
    size_t file_row = state->top_line + (size_t)i;
    vi_draw_row(state, i + 1, file_row);
  }

  vi_draw_status_bar(state);
  vi_draw_message_bar(state);

  int screen_row = (int)(state->cursor_row - state->top_line) + 1;
  int screen_col = (int)state->cursor_col + 1;
  move_cursor(screen_row, screen_col);

  show_cursor();
}


enum {
  KEY_ARROW_UP = 1000,
  KEY_ARROW_DOWN,
  KEY_ARROW_RIGHT,
  KEY_ARROW_LEFT,
  KEY_PAGE_UP,
  KEY_PAGE_DOWN,
  KEY_HOME,
  KEY_END,
  KEY_DEL
};


/**
 * Reads a key from the terminal.
 * @return Key code, -1 on error, -2 on terminal resize
 */
static int vi_read_key(void) {
  char c;
  int nread;
  while ((nread = (int)read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) return -1;
    if (term_resized) return -2;
  }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return KEY_HOME;
            case '3': return KEY_DEL;
            case '4': return KEY_END;
            case '5': return KEY_PAGE_UP;
            case '6': return KEY_PAGE_DOWN;
            case '7': return KEY_HOME;
            case '8': return KEY_END;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return KEY_ARROW_UP;
          case 'B': return KEY_ARROW_DOWN;
          case 'C': return KEY_ARROW_RIGHT;
          case 'D': return KEY_ARROW_LEFT;
          case 'H': return KEY_HOME;
          case 'F': return KEY_END;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
      }
    }
    return '\x1b';
  }

  return c;
}


/**
 * Checks if a character is a word character.
 * @param c Character to check
 * @return Non-zero if word character, 0 otherwise
 */
static int is_word_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
}


/**
 * Moves cursor forward by one word.
 * @param state Pointer to state structure
 */
static void vi_move_word_forward(vi_state_t *state) {
  vi_line_t *line = &state->lines[state->cursor_row];

  while (state->cursor_col < line->len &&
         is_word_char(line->text[state->cursor_col])) {
    state->cursor_col++;
  }

  while (state->cursor_col < line->len &&
         !is_word_char(line->text[state->cursor_col])) {
    state->cursor_col++;
  }

  if (state->cursor_col >= line->len &&
      state->cursor_row < state->line_count - 1) {
    state->cursor_row++;
    state->cursor_col = 0;
    line = &state->lines[state->cursor_row];
    while (state->cursor_col < line->len &&
           !is_word_char(line->text[state->cursor_col])) {
      state->cursor_col++;
    }
  }
}


/**
 * Moves cursor backward by one word.
 * @param state Pointer to state structure
 */
static void vi_move_word_backward(vi_state_t *state) {
  if (state->cursor_col == 0 && state->cursor_row > 0) {
    state->cursor_row--;
    state->cursor_col = state->lines[state->cursor_row].len;
  }

  vi_line_t *line = &state->lines[state->cursor_row];

  if (state->cursor_col > 0) state->cursor_col--;

  while (state->cursor_col > 0 &&
         !is_word_char(line->text[state->cursor_col])) {
    state->cursor_col--;
  }

  while (state->cursor_col > 0 &&
         is_word_char(line->text[state->cursor_col - 1])) {
    state->cursor_col--;
  }
}


/**
 * Yanks (copies) the current line to the yank buffer.
 * @param state Pointer to state structure
 */
static void vi_yank_line(vi_state_t *state) {
  vi_line_t *line = &state->lines[state->cursor_row];
  size_t copy_len = line->len;
  if (copy_len >= sizeof(state->yank_buf)) {
    copy_len = sizeof(state->yank_buf) - 1;
  }
  memcpy(state->yank_buf, line->text, copy_len);
  state->yank_buf[copy_len] = '\0';
  state->yank_is_line = 1;
  snprintf(state->status_msg, sizeof(state->status_msg), "1 line yanked");
}


/**
 * Pastes the yank buffer after the cursor.
 * @param state Pointer to state structure
 */
static void vi_paste_after(vi_state_t *state) {
  if (state->yank_buf[0] == '\0') return;

  if (state->yank_is_line) {
    vi_insert_line(state, state->cursor_row + 1);
    vi_line_set(&state->lines[state->cursor_row + 1],
                state->yank_buf, strlen(state->yank_buf));
    state->cursor_row++;
    state->cursor_col = 0;
  } else {
    vi_line_t *line = &state->lines[state->cursor_row];
    size_t pos = state->cursor_col;
    if (pos < line->len) pos++;
    for (size_t i = 0; state->yank_buf[i] != '\0'; i++) {
      vi_line_insert_char(line, pos + i, state->yank_buf[i]);
    }
    state->cursor_col = pos + strlen(state->yank_buf) - 1;
  }
}


/**
 * Pastes the yank buffer before the cursor.
 * @param state Pointer to state structure
 */
static void vi_paste_before(vi_state_t *state) {
  if (state->yank_buf[0] == '\0') return;

  if (state->yank_is_line) {
    vi_insert_line(state, state->cursor_row);
    vi_line_set(&state->lines[state->cursor_row],
                state->yank_buf, strlen(state->yank_buf));
    state->cursor_col = 0;
  } else {
    vi_line_t *line = &state->lines[state->cursor_row];
    size_t pos = state->cursor_col;
    for (size_t i = 0; state->yank_buf[i] != '\0'; i++) {
      vi_line_insert_char(line, pos + i, state->yank_buf[i]);
    }
  }
}


/**
 * Searches forward for the pattern in the command buffer.
 * @param state Pointer to state structure
 */
static void vi_search_forward(vi_state_t *state) {
  if (state->command_buf[0] == '\0') return;

  for (size_t row = state->cursor_row; row < state->line_count; row++) {
    vi_line_t *line = &state->lines[row];
    size_t start_col = (row == state->cursor_row) ? state->cursor_col + 1 : 0;

    for (size_t col = start_col; col < line->len; col++) {
      if (strncmp(&line->text[col], state->command_buf,
                  strlen(state->command_buf)) == 0) {
        state->cursor_row = row;
        state->cursor_col = col;
        snprintf(state->status_msg, sizeof(state->status_msg),
                 "/%.200s", state->command_buf);
        return;
      }
    }
  }

  for (size_t row = 0; row <= state->cursor_row; row++) {
    vi_line_t *line = &state->lines[row];
    size_t end_col = (row == state->cursor_row) ? state->cursor_col : line->len;

    for (size_t col = 0; col < end_col; col++) {
      if (strncmp(&line->text[col], state->command_buf,
                  strlen(state->command_buf)) == 0) {
        state->cursor_row = row;
        state->cursor_col = col;
        snprintf(state->status_msg, sizeof(state->status_msg),
                 "/%.200s (wrapped)", state->command_buf);
        return;
      }
    }
  }

  snprintf(state->status_msg, sizeof(state->status_msg),
           "Pattern not found: %.200s", state->command_buf);
}


/**
 * Processes a command mode command.
 * @param state Pointer to state structure
 * @return 1 to quit, 0 to continue
 */
static int vi_process_command(vi_state_t *state) {
  char *cmd = state->command_buf;

  if (strcmp(cmd, "q") == 0) {
    if (state->modified) {
      snprintf(state->status_msg, sizeof(state->status_msg),
               "No write since last change (use :q! to override)");
      return 0;
    }
    return 1;
  }

  if (strcmp(cmd, "q!") == 0) {
    return 1;
  }

  if (strcmp(cmd, "w") == 0) {
    vi_save_file(state);
    return 0;
  }

  if (strcmp(cmd, "wq") == 0 || strcmp(cmd, "x") == 0) {
    if (vi_save_file(state) == 0) {
      return 1;
    }
    return 0;
  }

  if (strncmp(cmd, "w ", 2) == 0) {
    free(state->filename);
    state->filename = strdup(cmd + 2);
    vi_save_file(state);
    return 0;
  }

  if (strncmp(cmd, "e ", 2) == 0) {
    if (vi_load_file(state, cmd + 2) == -1) {
      snprintf(state->status_msg, sizeof(state->status_msg),
               "Cannot open: %s", strerror(errno));
    }
    return 0;
  }

  snprintf(state->status_msg, sizeof(state->status_msg),
           "Not an editor command: %.200s", cmd);
  return 0;
}


/**
 * Handles a key press in normal mode.
 * @param state Pointer to state structure
 * @param key Key code
 * @return 1 to quit, 0 to continue
 */
static int vi_handle_normal_mode(vi_state_t *state, int key) {
  static int pending_g = 0;
  static int pending_d = 0;
  static int pending_y = 0;

  state->status_msg[0] = '\0';

  if (pending_g) {
    pending_g = 0;
    if (key == 'g') {
      state->cursor_row = 0;
      state->cursor_col = 0;
      return 0;
    }
    return 0;
  }

  if (pending_d) {
    pending_d = 0;
    if (key == 'd') {
      vi_yank_line(state);
      vi_delete_line(state, state->cursor_row);
      vi_clamp_cursor(state);
      snprintf(state->status_msg, sizeof(state->status_msg), "1 line deleted");
      return 0;
    }
    return 0;
  }

  if (pending_y) {
    pending_y = 0;
    if (key == 'y') {
      vi_yank_line(state);
      return 0;
    }
    return 0;
  }

  switch (key) {
    case 'h':
    case KEY_ARROW_LEFT:
      if (state->cursor_col > 0) state->cursor_col--;
      break;

    case 'j':
    case KEY_ARROW_DOWN:
      if (state->cursor_row < state->line_count - 1) state->cursor_row++;
      vi_clamp_cursor(state);
      break;

    case 'k':
    case KEY_ARROW_UP:
      if (state->cursor_row > 0) state->cursor_row--;
      vi_clamp_cursor(state);
      break;

    case 'l':
    case KEY_ARROW_RIGHT:
      if (state->cursor_col < state->lines[state->cursor_row].len - 1) {
        state->cursor_col++;
      }
      break;

    case '0':
    case KEY_HOME:
      state->cursor_col = 0;
      break;

    case '$':
    case KEY_END:
      if (state->lines[state->cursor_row].len > 0) {
        state->cursor_col = state->lines[state->cursor_row].len - 1;
      }
      break;

    case 'g':
      pending_g = 1;
      break;

    case 'G':
      state->cursor_row = state->line_count > 0 ? state->line_count - 1 : 0;
      state->cursor_col = 0;
      break;

    case 'w':
      vi_move_word_forward(state);
      vi_clamp_cursor(state);
      break;

    case 'b':
      vi_move_word_backward(state);
      vi_clamp_cursor(state);
      break;

    case 'i':
      state->mode = MODE_INSERT;
      break;

    case 'a':
      state->mode = MODE_INSERT;
      if (state->lines[state->cursor_row].len > 0) {
        state->cursor_col++;
      }
      break;

    case 'A':
      state->mode = MODE_INSERT;
      state->cursor_col = state->lines[state->cursor_row].len;
      break;

    case 'o':
      vi_insert_line(state, state->cursor_row + 1);
      state->cursor_row++;
      state->cursor_col = 0;
      state->mode = MODE_INSERT;
      break;

    case 'O':
      vi_insert_line(state, state->cursor_row);
      state->cursor_col = 0;
      state->mode = MODE_INSERT;
      break;

    case 'x':
    case KEY_DEL:
      if (state->lines[state->cursor_row].len > 0) {
        vi_line_delete_char(&state->lines[state->cursor_row],
                            state->cursor_col);
        state->modified = 1;
        vi_clamp_cursor(state);
      }
      break;

    case 'd':
      pending_d = 1;
      break;

    case 'y':
      pending_y = 1;
      break;

    case 'p':
      vi_paste_after(state);
      vi_clamp_cursor(state);
      break;

    case 'P':
      vi_paste_before(state);
      vi_clamp_cursor(state);
      break;

    case ':':
      state->mode = MODE_COMMAND;
      state->command_buf[0] = '\0';
      state->command_len = 0;
      break;

    case '/':
      state->mode = MODE_SEARCH;
      state->command_buf[0] = '\0';
      state->command_len = 0;
      break;

    case 'n':
      vi_search_forward(state);
      vi_clamp_cursor(state);
      vi_scroll_to_cursor(state);
      break;

    case KEY_PAGE_DOWN:
      state->cursor_row += (size_t)(state->rows - 2);
      if (state->cursor_row >= state->line_count) {
        state->cursor_row = state->line_count - 1;
      }
      vi_clamp_cursor(state);
      break;

    case KEY_PAGE_UP:
      if (state->cursor_row > (size_t)(state->rows - 2)) {
        state->cursor_row -= (size_t)(state->rows - 2);
      } else {
        state->cursor_row = 0;
      }
      vi_clamp_cursor(state);
      break;
  }

  return 0;
}


/**
 * Handles a key press in insert mode.
 * @param state Pointer to state structure
 * @param key Key code
 * @return 1 to quit, 0 to continue
 */
static int vi_handle_insert_mode(vi_state_t *state, int key) {
  vi_line_t *line = &state->lines[state->cursor_row];

  switch (key) {
    case '\x1b':
      state->mode = MODE_NORMAL;
      if (state->cursor_col > 0) state->cursor_col--;
      vi_clamp_cursor(state);
      break;

    case '\r':
    case '\n': {
      char *rest = line->text + state->cursor_col;
      size_t rest_len = line->len - state->cursor_col;

      vi_insert_line(state, state->cursor_row + 1);
      vi_line_set(&state->lines[state->cursor_row + 1], rest, rest_len);

      line->text[state->cursor_col] = '\0';
      line->len = state->cursor_col;

      state->cursor_row++;
      state->cursor_col = 0;
      break;
    }

    case 127:
    case '\x08':
      if (state->cursor_col > 0) {
        vi_line_delete_char(line, state->cursor_col - 1);
        state->cursor_col--;
        state->modified = 1;
      } else if (state->cursor_row > 0) {
        size_t prev_len = state->lines[state->cursor_row - 1].len;
        vi_line_ensure_capacity(&state->lines[state->cursor_row - 1],
                                prev_len + line->len);
        strcat(state->lines[state->cursor_row - 1].text, line->text);
        state->lines[state->cursor_row - 1].len += line->len;
        vi_delete_line(state, state->cursor_row);
        state->cursor_row--;
        state->cursor_col = prev_len;
      }
      break;

    case KEY_ARROW_LEFT:
      if (state->cursor_col > 0) state->cursor_col--;
      break;

    case KEY_ARROW_RIGHT:
      if (state->cursor_col < line->len) state->cursor_col++;
      break;

    case KEY_ARROW_UP:
      if (state->cursor_row > 0) state->cursor_row--;
      vi_clamp_cursor(state);
      break;

    case KEY_ARROW_DOWN:
      if (state->cursor_row < state->line_count - 1) state->cursor_row++;
      vi_clamp_cursor(state);
      break;

    default:
      if (key >= 32 && key < 127) {
        vi_line_insert_char(line, state->cursor_col, (char)key);
        state->cursor_col++;
        state->modified = 1;
      }
      break;
  }

  return 0;
}


/**
 * Handles a key press in command mode.
 * @param state Pointer to state structure
 * @param key Key code
 * @return 1 to quit, 0 to continue
 */
static int vi_handle_command_mode(vi_state_t *state, int key) {
  switch (key) {
    case '\x1b':
      state->mode = MODE_NORMAL;
      state->command_buf[0] = '\0';
      state->command_len = 0;
      break;

    case '\r':
    case '\n':
      state->mode = MODE_NORMAL;
      if (state->command_len > 0) {
        if (vi_process_command(state)) {
          return 1;
        }
      }
      state->command_buf[0] = '\0';
      state->command_len = 0;
      break;

    case 127:
    case '\x08':
      if (state->command_len > 0) {
        state->command_len--;
        state->command_buf[state->command_len] = '\0';
      } else {
        state->mode = MODE_NORMAL;
      }
      break;

    default:
      if (key >= 32 && key < 127 &&
          state->command_len < sizeof(state->command_buf) - 1) {
        state->command_buf[state->command_len++] = (char)key;
        state->command_buf[state->command_len] = '\0';
      }
      break;
  }

  return 0;
}


/**
 * Handles a key press in search mode.
 * @param state Pointer to state structure
 * @param key Key code
 * @return 1 to quit, 0 to continue
 */
static int vi_handle_search_mode(vi_state_t *state, int key) {
  switch (key) {
    case '\x1b':
      state->mode = MODE_NORMAL;
      state->command_buf[0] = '\0';
      state->command_len = 0;
      break;

    case '\r':
    case '\n':
      state->mode = MODE_NORMAL;
      if (state->command_len > 0) {
        vi_search_forward(state);
        vi_clamp_cursor(state);
        vi_scroll_to_cursor(state);
      }
      break;

    case 127:
    case '\x08':
      if (state->command_len > 0) {
        state->command_len--;
        state->command_buf[state->command_len] = '\0';
      } else {
        state->mode = MODE_NORMAL;
      }
      break;

    default:
      if (key >= 32 && key < 127 &&
          state->command_len < sizeof(state->command_buf) - 1) {
        state->command_buf[state->command_len++] = (char)key;
        state->command_buf[state->command_len] = '\0';
      }
      break;
  }

  return 0;
}


/**
 * Main entry point for the vi command.
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status (0 on success)
 */
static int vi_run(int argc, char **argv) {
  vi_args_t args;
  build_vi_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    vi_print_usage(stdout);
    cleanup_vi_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "vi");
    fprintf(stderr, "Try 'vi --help' for more information.\n");
    cleanup_vi_argtable(&args);
    return 1;
  }

  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    fprintf(stderr, "vi: requires a terminal\n");
    cleanup_vi_argtable(&args);
    return 1;
  }

  vi_state_t state;
  vi_state_init(&state);

  if (args.file->count > 0) {
    if (vi_load_file(&state, args.file->filename[0]) == -1) {
      fprintf(stderr, "vi: %s: %s\n", args.file->filename[0], strerror(errno));
      vi_state_free(&state);
      cleanup_vi_argtable(&args);
      return 1;
    }
  }

  if (enable_raw_mode() == -1) {
    fprintf(stderr, "vi: failed to enable raw mode\n");
    vi_state_free(&state);
    cleanup_vi_argtable(&args);
    return 1;
  }

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  /* Set up signal handlers */
  sa.sa_handler = handle_sigwinch;
  sigaction(SIGWINCH, &sa, NULL);

  sa.sa_handler = handle_sigtstp;
  sigaction(SIGTSTP, &sa, NULL);

  sa.sa_handler = handle_sigcont;
  sigaction(SIGCONT, &sa, NULL);

  sa.sa_handler = handle_sigterm;
  sigaction(SIGTERM, &sa, NULL);

  clear_screen();
  vi_draw_screen(&state);

  int quit = 0;
  while (!quit) {
    /* Handle terminal resize */
    if (term_resized) {
      term_resized = 0;
      get_terminal_size(&state.rows, &state.cols);
      vi_scroll_to_cursor(&state);
      vi_draw_screen(&state);
    }

    /* Handle SIGTSTP (external signal) */
    if (term_suspended) {
      term_suspended = 0;
      vi_suspend();
      vi_draw_screen(&state);
      continue;
    }

    /* Handle SIGTERM - save and exit gracefully */
    if (term_terminated) {
      vi_emergency_save(&state);
      quit = 1;
      continue;
    }

    int key = vi_read_key();
    if (key == -1) continue;
    if (key == -2) continue;

    /* Handle Ctrl+C (0x03) - cancel current operation, return to normal mode */
    if (key == 0x03) {
      state.mode = MODE_NORMAL;
      state.command_buf[0] = '\0';
      state.command_len = 0;
      snprintf(state.status_msg, sizeof(state.status_msg), "^C");
      vi_draw_screen(&state);
      continue;
    }

    /* Handle Ctrl+Z (0x1a) - suspend editor */
    if (key == 0x1a) {
      vi_suspend();
      vi_draw_screen(&state);
      continue;
    }

    switch (state.mode) {
      case MODE_NORMAL:
        quit = vi_handle_normal_mode(&state, key);
        break;
      case MODE_INSERT:
        quit = vi_handle_insert_mode(&state, key);
        break;
      case MODE_COMMAND:
        quit = vi_handle_command_mode(&state, key);
        break;
      case MODE_SEARCH:
        quit = vi_handle_search_mode(&state, key);
        break;
    }

    vi_scroll_to_cursor(&state);
    vi_draw_screen(&state);
  }

  clear_screen();
  move_cursor(1, 1);
  show_cursor();

  vi_state_free(&state);
  cleanup_vi_argtable(&args);

  return 0;
}


/** Command specification for vi */
const jshell_cmd_spec_t cmd_vi_spec = {
  .name = "vi",
  .summary = "edit files with vi-like interface",
  .long_help = "Edit FILE with a vi-like text editor. "
               "Supports normal, insert, and command modes. "
               "Basic navigation with hjkl, editing with i/a/o/x/dd/yy/p, "
               "and commands with :w, :q, :wq.",
  .type = CMD_EXTERNAL,
  .run = vi_run,
  .print_usage = vi_print_usage
};


/**
 * Registers the vi command with the shell command registry.
 */
void jshell_register_vi_command(void) {
  jshell_register_command(&cmd_vi_spec);
}
