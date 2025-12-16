/**
 * @file ftp_interactive.c
 * @brief FTP client interactive mode implementation.
 *
 * Provides a command-line interface for interacting with the FTP server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ftp_interactive.h"


/** Maximum length of an interactive command line. */
#define MAX_CMD_LINE 1024

/** Maximum number of arguments in a command. */
#define MAX_ARGS 8


/**
 * @brief Print available commands.
 *
 * @param json_output If true, output in JSON format.
 */
static void print_help(bool json_output) {
  if (json_output) {
    printf("{\"commands\":[");
    printf("{\"name\":\"ls\",\"usage\":\"ls [path]\",\"desc\":\"List directory\"},");
    printf("{\"name\":\"cd\",\"usage\":\"cd <path>\",\"desc\":\"Change directory\"},");
    printf("{\"name\":\"pwd\",\"usage\":\"pwd\",\"desc\":\"Print working directory\"},");
    printf("{\"name\":\"get\",\"usage\":\"get <remote> [local]\",\"desc\":\"Download file\"},");
    printf("{\"name\":\"put\",\"usage\":\"put <local> [remote]\",\"desc\":\"Upload file\"},");
    printf("{\"name\":\"mkdir\",\"usage\":\"mkdir <dir>\",\"desc\":\"Create directory\"},");
    printf("{\"name\":\"help\",\"usage\":\"help\",\"desc\":\"Show commands\"},");
    printf("{\"name\":\"quit\",\"usage\":\"quit\",\"desc\":\"Disconnect and exit\"}");
    printf("]}\n");
  } else {
    printf("Available commands:\n");
    printf("  ls [path]            List directory contents\n");
    printf("  cd <path>            Change directory\n");
    printf("  pwd                  Print working directory\n");
    printf("  get <remote> [local] Download file\n");
    printf("  put <local> [remote] Upload file\n");
    printf("  mkdir <dir>          Create directory\n");
    printf("  help                 Show this help\n");
    printf("  quit                 Disconnect and exit\n");
  }
}


/**
 * @brief Parse a command line into arguments.
 *
 * @param line Input line (modified in place).
 * @param argv Array to store argument pointers.
 * @param max_args Maximum number of arguments.
 * @return Number of arguments parsed.
 */
static int parse_args(char *line, char **argv, int max_args) {
  int argc = 0;
  char *p = line;

  while (*p && argc < max_args) {
    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) {
      p++;
    }
    if (!*p) {
      break;
    }

    /* Start of argument */
    argv[argc++] = p;

    /* Find end of argument */
    while (*p && !isspace((unsigned char)*p)) {
      p++;
    }
    if (*p) {
      *p++ = '\0';
    }
  }

  return argc;
}


/**
 * @brief Handle the ls command.
 *
 * @param session FTP session.
 * @param path Optional path to list.
 * @param json_output JSON output mode.
 */
static void handle_ls(ftp_session_t *session, const char *path,
                      bool json_output) {
  char *listing = NULL;

  if (ftp_list(session, path, &listing) < 0) {
    if (json_output) {
      printf("{\"action\":\"ls\",\"status\":\"error\","
             "\"message\":\"%s\"}\n", ftp_last_response(session));
    } else {
      fprintf(stderr, "ftp: list failed: %s\n", ftp_last_response(session));
    }
    return;
  }

  if (json_output) {
    /* Output listing as JSON - escape special characters */
    printf("{\"action\":\"ls\",\"status\":\"ok\",\"listing\":\"");
    if (listing) {
      for (const char *p = listing; *p; p++) {
        switch (*p) {
          case '"':  printf("\\\""); break;
          case '\\': printf("\\\\"); break;
          case '\n': printf("\\n"); break;
          case '\r': break;
          case '\t': printf("\\t"); break;
          default:
            if ((unsigned char)*p >= 32) {
              putchar(*p);
            }
            break;
        }
      }
    }
    printf("\"}\n");
  } else {
    if (listing && *listing) {
      printf("%s", listing);
      /* Ensure trailing newline */
      size_t len = strlen(listing);
      if (len > 0 && listing[len - 1] != '\n') {
        putchar('\n');
      }
    }
  }

  free(listing);
}


/**
 * @brief Handle the cd command.
 *
 * @param session FTP session.
 * @param path Directory to change to.
 * @param json_output JSON output mode.
 */
static void handle_cd(ftp_session_t *session, const char *path,
                      bool json_output) {
  if (!path || !*path) {
    if (json_output) {
      printf("{\"action\":\"cd\",\"status\":\"error\","
             "\"message\":\"missing path argument\"}\n");
    } else {
      fprintf(stderr, "ftp: cd: missing path argument\n");
    }
    return;
  }

  if (ftp_cd(session, path) < 0) {
    if (json_output) {
      printf("{\"action\":\"cd\",\"status\":\"error\","
             "\"message\":\"%s\"}\n", ftp_last_response(session));
    } else {
      fprintf(stderr, "ftp: cd failed: %s\n", ftp_last_response(session));
    }
    return;
  }

  if (json_output) {
    printf("{\"action\":\"cd\",\"status\":\"ok\",\"path\":\"%s\"}\n", path);
  } else {
    printf("Changed to %s\n", path);
  }
}


/**
 * @brief Handle the pwd command.
 *
 * @param session FTP session.
 * @param json_output JSON output mode.
 */
static void handle_pwd(ftp_session_t *session, bool json_output) {
  char path[512];

  if (ftp_pwd(session, path, sizeof(path)) < 0) {
    if (json_output) {
      printf("{\"action\":\"pwd\",\"status\":\"error\","
             "\"message\":\"%s\"}\n", ftp_last_response(session));
    } else {
      fprintf(stderr, "ftp: pwd failed: %s\n", ftp_last_response(session));
    }
    return;
  }

  if (json_output) {
    printf("{\"action\":\"pwd\",\"status\":\"ok\",\"path\":\"%s\"}\n", path);
  } else {
    printf("%s\n", path);
  }
}


/**
 * @brief Handle the get command.
 *
 * @param session FTP session.
 * @param remote Remote filename.
 * @param local Local filename (NULL to use remote name).
 * @param json_output JSON output mode.
 */
static void handle_get(ftp_session_t *session, const char *remote,
                       const char *local, bool json_output) {
  if (!remote || !*remote) {
    if (json_output) {
      printf("{\"action\":\"get\",\"status\":\"error\","
             "\"message\":\"missing remote filename\"}\n");
    } else {
      fprintf(stderr, "ftp: get: missing remote filename\n");
    }
    return;
  }

  const char *local_name = local && *local ? local : remote;

  if (ftp_get(session, remote, local_name) < 0) {
    if (json_output) {
      printf("{\"action\":\"get\",\"status\":\"error\","
             "\"remote\":\"%s\",\"message\":\"%s\"}\n",
             remote, ftp_last_response(session));
    } else {
      fprintf(stderr, "ftp: get failed: %s\n", ftp_last_response(session));
    }
    return;
  }

  if (json_output) {
    printf("{\"action\":\"get\",\"status\":\"ok\","
           "\"remote\":\"%s\",\"local\":\"%s\"}\n", remote, local_name);
  } else {
    printf("Downloaded %s -> %s\n", remote, local_name);
  }
}


/**
 * @brief Handle the put command.
 *
 * @param session FTP session.
 * @param local Local filename.
 * @param remote Remote filename (NULL to use local name).
 * @param json_output JSON output mode.
 */
static void handle_put(ftp_session_t *session, const char *local,
                       const char *remote, bool json_output) {
  if (!local || !*local) {
    if (json_output) {
      printf("{\"action\":\"put\",\"status\":\"error\","
             "\"message\":\"missing local filename\"}\n");
    } else {
      fprintf(stderr, "ftp: put: missing local filename\n");
    }
    return;
  }

  /* Extract basename for default remote name */
  const char *base = strrchr(local, '/');
  base = base ? base + 1 : local;
  const char *remote_name = remote && *remote ? remote : base;

  if (ftp_put(session, local, remote_name) < 0) {
    if (json_output) {
      printf("{\"action\":\"put\",\"status\":\"error\","
             "\"local\":\"%s\",\"message\":\"%s\"}\n",
             local, ftp_last_response(session));
    } else {
      fprintf(stderr, "ftp: put failed: %s\n", ftp_last_response(session));
    }
    return;
  }

  if (json_output) {
    printf("{\"action\":\"put\",\"status\":\"ok\","
           "\"local\":\"%s\",\"remote\":\"%s\"}\n", local, remote_name);
  } else {
    printf("Uploaded %s -> %s\n", local, remote_name);
  }
}


/**
 * @brief Handle the mkdir command.
 *
 * @param session FTP session.
 * @param dirname Directory name to create.
 * @param json_output JSON output mode.
 */
static void handle_mkdir(ftp_session_t *session, const char *dirname,
                         bool json_output) {
  if (!dirname || !*dirname) {
    if (json_output) {
      printf("{\"action\":\"mkdir\",\"status\":\"error\","
             "\"message\":\"missing directory name\"}\n");
    } else {
      fprintf(stderr, "ftp: mkdir: missing directory name\n");
    }
    return;
  }

  if (ftp_mkdir(session, dirname) < 0) {
    if (json_output) {
      printf("{\"action\":\"mkdir\",\"status\":\"error\","
             "\"dir\":\"%s\",\"message\":\"%s\"}\n",
             dirname, ftp_last_response(session));
    } else {
      fprintf(stderr, "ftp: mkdir failed: %s\n", ftp_last_response(session));
    }
    return;
  }

  if (json_output) {
    printf("{\"action\":\"mkdir\",\"status\":\"ok\",\"dir\":\"%s\"}\n",
           dirname);
  } else {
    printf("Created directory %s\n", dirname);
  }
}


int ftp_interactive(ftp_session_t *session, bool json_output) {
  char line[MAX_CMD_LINE];
  char *argv[MAX_ARGS];
  int argc;

  if (!json_output) {
    printf("Type 'help' for available commands.\n");
  }

  while (1) {
    /* Print prompt */
    if (!json_output) {
      printf("ftp> ");
      fflush(stdout);
    }

    /* Read command line */
    if (!fgets(line, sizeof(line), stdin)) {
      /* EOF */
      if (!json_output) {
        printf("\n");
      }
      break;
    }

    /* Remove trailing newline */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    /* Parse arguments */
    argc = parse_args(line, argv, MAX_ARGS);
    if (argc == 0) {
      continue;
    }

    const char *cmd = argv[0];

    /* Dispatch command */
    if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
      if (json_output) {
        printf("{\"action\":\"quit\",\"status\":\"ok\"}\n");
      } else {
        printf("Goodbye.\n");
      }
      break;
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
      print_help(json_output);
    } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
      handle_ls(session, argc > 1 ? argv[1] : NULL, json_output);
    } else if (strcmp(cmd, "cd") == 0) {
      handle_cd(session, argc > 1 ? argv[1] : NULL, json_output);
    } else if (strcmp(cmd, "pwd") == 0) {
      handle_pwd(session, json_output);
    } else if (strcmp(cmd, "get") == 0) {
      handle_get(session, argc > 1 ? argv[1] : NULL,
                 argc > 2 ? argv[2] : NULL, json_output);
    } else if (strcmp(cmd, "put") == 0) {
      handle_put(session, argc > 1 ? argv[1] : NULL,
                 argc > 2 ? argv[2] : NULL, json_output);
    } else if (strcmp(cmd, "mkdir") == 0) {
      handle_mkdir(session, argc > 1 ? argv[1] : NULL, json_output);
    } else {
      if (json_output) {
        printf("{\"action\":\"%s\",\"status\":\"error\","
               "\"message\":\"unknown command\"}\n", cmd);
      } else {
        fprintf(stderr, "ftp: unknown command: %s\n", cmd);
        fprintf(stderr, "Type 'help' for available commands.\n");
      }
    }
  }

  return 0;
}
