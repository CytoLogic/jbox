#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>

#include "pkg_utils.h"


char *pkg_get_home_dir(void) {
  const char *home = getenv("HOME");
  if (home == NULL) {
    return NULL;
  }

  size_t len = strlen(home) + strlen("/.jshell") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    return NULL;
  }

  snprintf(path, len, "%s/.jshell", home);
  return path;
}


char *pkg_get_pkgs_dir(void) {
  char *home = pkg_get_home_dir();
  if (home == NULL) {
    return NULL;
  }

  size_t len = strlen(home) + strlen("/pkgs") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    free(home);
    return NULL;
  }

  snprintf(path, len, "%s/pkgs", home);
  free(home);
  return path;
}


char *pkg_get_bin_dir(void) {
  char *home = pkg_get_home_dir();
  if (home == NULL) {
    return NULL;
  }

  size_t len = strlen(home) + strlen("/bin") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    free(home);
    return NULL;
  }

  snprintf(path, len, "%s/bin", home);
  free(home);
  return path;
}


char *pkg_get_db_path(void) {
  char *pkgs = pkg_get_pkgs_dir();
  if (pkgs == NULL) {
    return NULL;
  }

  size_t len = strlen(pkgs) + strlen("/pkgdb.json") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    free(pkgs);
    return NULL;
  }

  snprintf(path, len, "%s/pkgdb.json", pkgs);
  free(pkgs);
  return path;
}


char *pkg_get_db_path_txt(void) {
  char *home = pkg_get_home_dir();
  if (home == NULL) {
    return NULL;
  }

  size_t len = strlen(home) + strlen("/pkgdb.txt") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    free(home);
    return NULL;
  }

  snprintf(path, len, "%s/pkgdb.txt", home);
  free(home);
  return path;
}


char *pkg_get_tmp_dir(void) {
  char *pkgs = pkg_get_pkgs_dir();
  if (pkgs == NULL) {
    return NULL;
  }

  size_t len = strlen(pkgs) + strlen("/_tmp") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    free(pkgs);
    return NULL;
  }

  snprintf(path, len, "%s/_tmp", pkgs);
  free(pkgs);
  return path;
}


static int ensure_dir(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return 0;
    }
    return -1;
  }

  if (mkdir(path, 0755) != 0) {
    return -1;
  }
  return 0;
}


int pkg_ensure_dirs(void) {
  char *home = pkg_get_home_dir();
  if (home == NULL) {
    return -1;
  }

  if (ensure_dir(home) != 0) {
    free(home);
    return -1;
  }
  free(home);

  char *pkgs = pkg_get_pkgs_dir();
  if (pkgs == NULL) {
    return -1;
  }

  if (ensure_dir(pkgs) != 0) {
    free(pkgs);
    return -1;
  }
  free(pkgs);

  char *bin = pkg_get_bin_dir();
  if (bin == NULL) {
    return -1;
  }

  if (ensure_dir(bin) != 0) {
    free(bin);
    return -1;
  }
  free(bin);

  return 0;
}


int pkg_run_command(char *const argv[]) {
  pid_t pid = fork();

  if (pid < 0) {
    return -1;
  }

  if (pid == 0) {
    execvp(argv[0], argv);
    _exit(127);
  }

  int status;
  if (waitpid(pid, &status, 0) < 0) {
    return -1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  return -1;
}


int pkg_remove_dir_recursive(const char *path) {
  DIR *dir = opendir(path);
  if (dir == NULL) {
    if (errno == ENOENT) {
      return 0;
    }
    return -1;
  }

  struct dirent *entry;
  int result = 0;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    size_t len = strlen(path) + strlen(entry->d_name) + 2;
    char *child_path = malloc(len);
    if (child_path == NULL) {
      result = -1;
      break;
    }

    snprintf(child_path, len, "%s/%s", path, entry->d_name);

    struct stat st;
    if (lstat(child_path, &st) != 0) {
      free(child_path);
      result = -1;
      break;
    }

    if (S_ISDIR(st.st_mode)) {
      if (pkg_remove_dir_recursive(child_path) != 0) {
        free(child_path);
        result = -1;
        break;
      }
    } else {
      if (unlink(child_path) != 0) {
        free(child_path);
        result = -1;
        break;
      }
    }

    free(child_path);
  }

  closedir(dir);

  if (result == 0) {
    if (rmdir(path) != 0) {
      return -1;
    }
  }

  return result;
}


char *pkg_read_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    return NULL;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }

  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }

  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }

  char *content = malloc((size_t)size + 1);
  if (content == NULL) {
    fclose(f);
    return NULL;
  }

  size_t read = fread(content, 1, (size_t)size, f);
  fclose(f);

  content[read] = '\0';
  return content;
}


int pkg_ensure_tmp_dir(void) {
  if (pkg_ensure_dirs() != 0) {
    return -1;
  }

  char *tmp = pkg_get_tmp_dir();
  if (tmp == NULL) {
    return -1;
  }

  int result = ensure_dir(tmp);
  free(tmp);
  return result;
}


int pkg_cleanup_tmp_dir(void) {
  char *tmp = pkg_get_tmp_dir();
  if (tmp == NULL) {
    return -1;
  }

  int result = pkg_remove_dir_recursive(tmp);
  free(tmp);
  return result;
}
