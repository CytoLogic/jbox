#ifndef JSHELL_PATH_H
#define JSHELL_PATH_H


// Initialize path system
// - Creates ~/.jshell/bin if it doesn't exist
// - Prepends ~/.jshell/bin to PATH
void jshell_init_path(void);

// Get the jshell bin directory path (~/.jshell/bin)
// Returns a pointer to an internal static buffer (do not free)
const char* jshell_get_bin_dir(void);

// Resolve a command name to its full path
// Returns:
//   - Full path to executable in ~/.jshell/bin if found
//   - Full path from system PATH if found
//   - NULL if not found anywhere
// Caller is responsible for freeing the returned string
char* jshell_resolve_command(const char* cmd_name);

// Cleanup path system resources
void jshell_cleanup_path(void);


#endif
