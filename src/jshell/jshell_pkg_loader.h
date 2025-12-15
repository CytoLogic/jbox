#ifndef JSHELL_PKG_LOADER_H
#define JSHELL_PKG_LOADER_H

// Load installed packages from pkgdb.json and register commands
// Returns number of packages loaded, or -1 on error
int jshell_load_installed_packages(void);

// Reload packages (unregister all, then load fresh)
// Returns number of packages loaded, or -1 on error
int jshell_reload_packages(void);

#endif
