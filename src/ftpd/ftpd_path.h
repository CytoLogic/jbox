/**
 * @file ftpd_path.h
 * @brief FTP path resolution and security.
 *
 * This module provides secure path resolution to prevent
 * directory traversal attacks. All paths are resolved relative
 * to the server root and validated to ensure they don't escape.
 */

#ifndef FTPD_PATH_H
#define FTPD_PATH_H

#include "ftpd.h"


/**
 * @brief Resolve a path securely within the server root.
 *
 * Takes a path (absolute or relative) and resolves it to an
 * absolute path within the server root. If the path would
 * escape the server root (e.g., via ".."), returns NULL.
 *
 * The returned path is always canonical (no "." or ".." components).
 *
 * @param client Pointer to client structure (for cwd).
 * @param path Path to resolve (may be NULL for cwd).
 * @param server_root Absolute path to server root directory.
 * @return Allocated canonical path string, or NULL if invalid.
 *         Caller must free the returned string.
 */
char *ftpd_resolve_path(ftpd_client_t *client, const char *path,
                        const char *server_root);


/**
 * @brief Check if a path is within the server root.
 *
 * Verifies that the given absolute path is a child of
 * (or equal to) the server root directory.
 *
 * @param path Absolute path to check.
 * @param server_root Absolute path to server root.
 * @return true if path is within root, false otherwise.
 */
bool ftpd_path_is_safe(const char *path, const char *server_root);


/**
 * @brief Get the display path for a file.
 *
 * Converts an absolute path to a path relative to the server root
 * for display to the client. The server root is represented as "/".
 *
 * @param abspath Absolute path to convert.
 * @param server_root Server root directory path.
 * @param buf Buffer to store the display path.
 * @param bufsize Size of the buffer.
 * @return Pointer to buf on success, NULL on error.
 */
char *ftpd_path_to_display(const char *abspath, const char *server_root,
                           char *buf, size_t bufsize);


/**
 * @brief Join two path components.
 *
 * Concatenates two path components with a "/" separator.
 *
 * @param base Base path.
 * @param name Name to append.
 * @param buf Buffer to store result.
 * @param bufsize Size of buffer.
 * @return Pointer to buf on success, NULL if buffer too small.
 */
char *ftpd_path_join(const char *base, const char *name,
                     char *buf, size_t bufsize);


#endif /* FTPD_PATH_H */
