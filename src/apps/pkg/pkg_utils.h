#ifndef PKG_UTILS_H
#define PKG_UTILS_H

// Returns path to ~/.jshell (caller must free)
char *pkg_get_home_dir(void);

// Returns path to ~/.jshell/pkgs (caller must free)
char *pkg_get_pkgs_dir(void);

// Returns path to ~/.jshell/bin (caller must free)
char *pkg_get_bin_dir(void);

// Returns path to ~/.jshell/pkgs/pkgdb.json (caller must free)
char *pkg_get_db_path(void);

// Returns path to ~/.jshell/pkgdb.txt (legacy, caller must free)
char *pkg_get_db_path_txt(void);

// Returns path to ~/.jshell/pkgs/_tmp (caller must free)
char *pkg_get_tmp_dir(void);

// Creates ~/.jshell, ~/.jshell/pkgs, ~/.jshell/bin if needed
// Returns 0 on success, -1 on error
int pkg_ensure_dirs(void);

// Ensures ~/.jshell/pkgs/_tmp exists
// Returns 0 on success, -1 on error
int pkg_ensure_tmp_dir(void);

// Cleans up ~/.jshell/pkgs/_tmp contents
// Returns 0 on success, -1 on error
int pkg_cleanup_tmp_dir(void);

// Fork/exec wrapper - runs command and waits for completion
// Returns exit status of child, or -1 on fork/exec error
int pkg_run_command(char *const argv[]);

// Recursively remove a directory
// Returns 0 on success, -1 on error
int pkg_remove_dir_recursive(const char *path);

// Read entire file into allocated string (caller must free)
// Returns NULL on error
char *pkg_read_file(const char *path);

#endif
