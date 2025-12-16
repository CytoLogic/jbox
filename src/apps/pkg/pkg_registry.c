/** @file pkg_registry.c
 *  @brief Package registry client for fetching and downloading packages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#include "pkg_registry.h"
#include "pkg_json.h"


/** Buffer structure for HTTP response data. */
typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} response_buffer_t;


/** CURL write callback for buffering response data.
 *  @param contents Data received
 *  @param size Size of each element
 *  @param nmemb Number of elements
 *  @param userp User pointer (response_buffer_t)
 *  @return Number of bytes processed
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  response_buffer_t *buf = (response_buffer_t *)userp;

  if (buf->size + realsize >= buf->capacity) {
    size_t new_capacity = buf->capacity * 2;
    if (new_capacity < buf->size + realsize + 1) {
      new_capacity = buf->size + realsize + 1;
    }
    char *new_data = realloc(buf->data, new_capacity);
    if (!new_data) {
      return 0;
    }
    buf->data = new_data;
    buf->capacity = new_capacity;
  }

  memcpy(buf->data + buf->size, contents, realsize);
  buf->size += realsize;
  buf->data[buf->size] = '\0';

  return realsize;
}


/** CURL write callback for writing to a file.
 *  @param contents Data received
 *  @param size Size of each element
 *  @param nmemb Number of elements
 *  @param userp User pointer (FILE*)
 *  @return Number of bytes written
 */
static size_t file_write_callback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
  FILE *fp = (FILE *)userp;
  return fwrite(contents, size, nmemb, fp);
}


/** Gets the package registry URL.
 *  @return Registry URL (from JSHELL_PKG_REGISTRY env var or default)
 */
const char *pkg_registry_get_url(void) {
  const char *env_url = getenv("JSHELL_PKG_REGISTRY");
  if (env_url && env_url[0] != '\0') {
    return env_url;
  }
  return PKG_REGISTRY_DEFAULT_URL;
}


/** Fetches JSON content from a URL using CURL.
 *  @param url URL to fetch
 *  @return Allocated JSON string, or NULL on error. Caller must free.
 */
static char *fetch_json(const char *url) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    return NULL;
  }

  response_buffer_t response = {
    .data = malloc(4096),
    .size = 0,
    .capacity = 4096
  };
  if (!response.data) {
    curl_easy_cleanup(curl);
    return NULL;
  }
  response.data[0] = '\0';

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "jbox-pkg/1.0");

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_easy_cleanup(curl);

  if (res != CURLE_OK || http_code >= 400) {
    free(response.data);
    return NULL;
  }

  return response.data;
}


/** Extracts a string value from JSON by key.
 *  @param json JSON string to search
 *  @param key Key name to find
 *  @return Allocated value string, or NULL if not found. Caller must free.
 */
static char *json_get_string(const char *json, const char *key) {
  char search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *key_pos = strstr(json, search);
  if (!key_pos) return NULL;

  const char *colon = strchr(key_pos + strlen(search), ':');
  if (!colon) return NULL;

  const char *p = colon + 1;
  while (*p && isspace((unsigned char)*p)) p++;

  if (*p != '"') return NULL;
  p++;

  const char *end = p;
  while (*end && *end != '"') {
    if (*end == '\\' && *(end + 1)) {
      end += 2;
    } else {
      end++;
    }
  }

  size_t len = (size_t)(end - p);
  char *result = malloc(len + 1);
  if (!result) return NULL;

  // Copy with basic escape handling
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if (p[i] == '\\' && i + 1 < len) {
      switch (p[i + 1]) {
        case 'n': result[j++] = '\n'; i++; break;
        case 't': result[j++] = '\t'; i++; break;
        case 'r': result[j++] = '\r'; i++; break;
        case '"': result[j++] = '"'; i++; break;
        case '\\': result[j++] = '\\'; i++; break;
        default: result[j++] = p[i]; break;
      }
    } else {
      result[j++] = p[i];
    }
  }
  result[j] = '\0';

  return result;
}


/** Parses a package registry entry from JSON object.
 *  @param json JSON string containing package object
 *  @return Allocated PkgRegistryEntry, or NULL on error. Caller must free with pkg_registry_entry_free.
 */
static PkgRegistryEntry *parse_package_object(const char *json) {
  PkgRegistryEntry *entry = calloc(1, sizeof(PkgRegistryEntry));
  if (!entry) return NULL;

  entry->name = json_get_string(json, "name");
  entry->latest_version = json_get_string(json, "latestVersion");
  entry->description = json_get_string(json, "description");
  entry->download_url = json_get_string(json, "downloadUrl");

  if (!entry->name) {
    pkg_registry_entry_free(entry);
    return NULL;
  }

  return entry;
}


/** Fetches all available packages from the registry.
 *  @return Allocated PkgRegistryList, or NULL on error. Caller must free with pkg_registry_list_free.
 */
PkgRegistryList *pkg_registry_fetch_all(void) {
  const char *base_url = pkg_registry_get_url();
  char url[512];
  snprintf(url, sizeof(url), "%s/packages", base_url);

  char *json = fetch_json(url);
  if (!json) return NULL;

  // Check for status: ok
  char *status = json_get_string(json, "status");
  if (!status || strcmp(status, "ok") != 0) {
    free(status);
    free(json);
    return NULL;
  }
  free(status);

  PkgRegistryList *list = calloc(1, sizeof(PkgRegistryList));
  if (!list) {
    free(json);
    return NULL;
  }

  // Find packages array
  const char *packages_start = strstr(json, "\"packages\"");
  if (!packages_start) {
    free(json);
    free(list);
    return NULL;
  }

  const char *arr_start = strchr(packages_start, '[');
  if (!arr_start) {
    free(json);
    free(list);
    return NULL;
  }

  // Parse each object in the array
  const char *p = arr_start + 1;
  while (*p) {
    while (*p && isspace((unsigned char)*p)) p++;

    if (*p == ']') break;
    if (*p == ',') { p++; continue; }

    if (*p == '{') {
      // Find matching closing brace
      int depth = 1;
      const char *obj_start = p;
      p++;
      while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        else if (*p == '"') {
          p++;
          while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) p++;
            p++;
          }
        }
        if (*p) p++;
      }

      size_t obj_len = (size_t)(p - obj_start);
      char *obj_json = malloc(obj_len + 1);
      if (obj_json) {
        memcpy(obj_json, obj_start, obj_len);
        obj_json[obj_len] = '\0';

        PkgRegistryEntry *entry = parse_package_object(obj_json);
        free(obj_json);

        if (entry) {
          if (list->count >= list->capacity) {
            int new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
            PkgRegistryEntry *new_entries = realloc(list->entries,
              (size_t)new_cap * sizeof(PkgRegistryEntry));
            if (!new_entries) {
              pkg_registry_entry_free(entry);
              break;
            }
            list->entries = new_entries;
            list->capacity = new_cap;
          }
          list->entries[list->count++] = *entry;
          free(entry);
        }
      }
    } else {
      p++;
    }
  }

  free(json);
  return list;
}


/** Fetches a specific package from the registry by name.
 *  @param name Package name
 *  @return Allocated PkgRegistryEntry, or NULL on error. Caller must free with pkg_registry_entry_free.
 */
PkgRegistryEntry *pkg_registry_fetch_package(const char *name) {
  if (!name) return NULL;

  const char *base_url = pkg_registry_get_url();
  char url[512];
  snprintf(url, sizeof(url), "%s/packages/%s", base_url, name);

  char *json = fetch_json(url);
  if (!json) return NULL;

  // Check for status: ok
  char *status = json_get_string(json, "status");
  if (!status || strcmp(status, "ok") != 0) {
    free(status);
    free(json);
    return NULL;
  }
  free(status);

  // Find package object
  const char *pkg_start = strstr(json, "\"package\"");
  if (!pkg_start) {
    free(json);
    return NULL;
  }

  const char *obj_start = strchr(pkg_start, '{');
  if (!obj_start) {
    free(json);
    return NULL;
  }

  PkgRegistryEntry *entry = parse_package_object(obj_start);
  free(json);

  return entry;
}


/** Searches the registry for packages matching a query.
 *  @param query Search query string
 *  @return Allocated PkgRegistryList with matching packages, or NULL on error. Caller must free with pkg_registry_list_free.
 */
PkgRegistryList *pkg_registry_search(const char *query) {
  if (!query) return NULL;

  // Fetch all packages and filter by query
  PkgRegistryList *all = pkg_registry_fetch_all();
  if (!all) return NULL;

  PkgRegistryList *results = calloc(1, sizeof(PkgRegistryList));
  if (!results) {
    pkg_registry_list_free(all);
    return NULL;
  }

  // Convert query to lowercase for case-insensitive search
  size_t query_len = strlen(query);
  char *query_lower = malloc(query_len + 1);
  if (!query_lower) {
    free(results);
    pkg_registry_list_free(all);
    return NULL;
  }
  for (size_t i = 0; i <= query_len; i++) {
    query_lower[i] = (char)tolower((unsigned char)query[i]);
  }

  for (int i = 0; i < all->count; i++) {
    PkgRegistryEntry *entry = &all->entries[i];
    bool match = false;

    // Check name
    if (entry->name) {
      char *name_lower = strdup(entry->name);
      if (name_lower) {
        for (char *p = name_lower; *p; p++) {
          *p = (char)tolower((unsigned char)*p);
        }
        if (strstr(name_lower, query_lower)) {
          match = true;
        }
        free(name_lower);
      }
    }

    // Check description
    if (!match && entry->description) {
      char *desc_lower = strdup(entry->description);
      if (desc_lower) {
        for (char *p = desc_lower; *p; p++) {
          *p = (char)tolower((unsigned char)*p);
        }
        if (strstr(desc_lower, query_lower)) {
          match = true;
        }
        free(desc_lower);
      }
    }

    if (match) {
      if (results->count >= results->capacity) {
        int new_cap = results->capacity == 0 ? 16 : results->capacity * 2;
        PkgRegistryEntry *new_entries = realloc(results->entries,
          (size_t)new_cap * sizeof(PkgRegistryEntry));
        if (!new_entries) break;
        results->entries = new_entries;
        results->capacity = new_cap;
      }

      // Copy the entry
      PkgRegistryEntry *dest = &results->entries[results->count];
      dest->name = entry->name ? strdup(entry->name) : NULL;
      dest->latest_version = entry->latest_version ?
                             strdup(entry->latest_version) : NULL;
      dest->description = entry->description ? strdup(entry->description) : NULL;
      dest->download_url = entry->download_url ?
                           strdup(entry->download_url) : NULL;
      results->count++;
    }
  }

  free(query_lower);
  pkg_registry_list_free(all);

  return results;
}


/** Downloads a file from a URL to a local path.
 *  @param url URL to download from
 *  @param dest_path Destination file path
 *  @return 0 on success, -1 on error
 */
int pkg_registry_download(const char *url, const char *dest_path) {
  if (!url || !dest_path) return -1;

  FILE *fp = fopen(dest_path, "wb");
  if (!fp) return -1;

  CURL *curl = curl_easy_init();
  if (!curl) {
    fclose(fp);
    return -1;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "jbox-pkg/1.0");

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_easy_cleanup(curl);
  fclose(fp);

  if (res != CURLE_OK || http_code >= 400) {
    remove(dest_path);
    return -1;
  }

  return 0;
}


/** Frees a package registry entry.
 *  @param entry Pointer to entry to free
 */
void pkg_registry_entry_free(PkgRegistryEntry *entry) {
  if (!entry) return;
  free(entry->name);
  free(entry->latest_version);
  free(entry->description);
  free(entry->download_url);
  free(entry);
}


/** Frees a package registry list and all its entries.
 *  @param list Pointer to list to free
 */
void pkg_registry_list_free(PkgRegistryList *list) {
  if (!list) return;
  for (int i = 0; i < list->count; i++) {
    free(list->entries[i].name);
    free(list->entries[i].latest_version);
    free(list->entries[i].description);
    free(list->entries[i].download_url);
  }
  free(list->entries);
  free(list);
}


/** Compares two semantic version strings.
 *  @param v1 First version string (e.g., "1.2.3")
 *  @param v2 Second version string
 *  @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
int pkg_version_compare(const char *v1, const char *v2) {
  if (!v1 && !v2) return 0;
  if (!v1) return -1;
  if (!v2) return 1;

  // Parse major.minor.patch
  int v1_parts[3] = {0, 0, 0};
  int v2_parts[3] = {0, 0, 0};

  sscanf(v1, "%d.%d.%d", &v1_parts[0], &v1_parts[1], &v1_parts[2]);
  sscanf(v2, "%d.%d.%d", &v2_parts[0], &v2_parts[1], &v2_parts[2]);

  for (int i = 0; i < 3; i++) {
    if (v1_parts[i] < v2_parts[i]) return -1;
    if (v1_parts[i] > v2_parts[i]) return 1;
  }

  return 0;
}
