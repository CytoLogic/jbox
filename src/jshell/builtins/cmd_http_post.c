/**
 * @file cmd_http_post.c
 * @brief HTTP POST command implementation using libcurl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "argtable3.h"
#include "cmd_http_post.h"
#include "jshell/jshell_signals.h"


/**
 * Argument table structure for http-post command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_str *headers;
  struct arg_str *data;
  struct arg_lit *json_output;
  struct arg_str *url;
  struct arg_end *end;
  void *argtable[6];
} http_post_args_t;


/**
 * Buffer for accumulating HTTP response body data.
 */
typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} response_buffer_t;


/**
 * Buffer for accumulating HTTP response headers.
 */
typedef struct {
  char **headers;
  size_t count;
  size_t capacity;
} header_buffer_t;


/**
 * Builds the argtable3 argument table for http-post command.
 * @param args Pointer to http_post_args_t structure to initialize
 */
static void build_http_post_argtable(http_post_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->headers = arg_strn("H", "header", "KEY:VALUE", 0, 20,
                           "add header to request (repeatable)");
  args->data = arg_str0("d", "data", "DATA", "request body data");
  args->json_output = arg_lit0(NULL, "json", "output response as JSON");
  args->url = arg_str1(NULL, NULL, "URL", "URL to post to");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->headers;
  args->argtable[2] = args->data;
  args->argtable[3] = args->json_output;
  args->argtable[4] = args->url;
  args->argtable[5] = args->end;
}


/**
 * Frees memory allocated for the argtable3 argument table.
 * @param args Pointer to http_post_args_t structure to clean up
 */
static void cleanup_http_post_argtable(http_post_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the http-post command.
 * @param out Output stream (typically stdout or stderr)
 */
static void http_post_print_usage(FILE *out) {
  http_post_args_t args;
  build_http_post_argtable(&args);
  fprintf(out, "Usage: http-post");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Send HTTP POST request to a URL.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-25s %s\n");
  fprintf(out, "\nExamples:\n");
  fprintf(out, "  http-post -d '{\"key\":\"value\"}' https://api.example.com\n");
  fprintf(out, "  http-post -H \"Content-Type: application/json\" "
               "-d '{\"name\":\"test\"}' https://api.example.com\n");
  fprintf(out, "  http-post --json -d 'data' https://example.com\n");
  cleanup_http_post_argtable(&args);
}


/**
 * Callback function for libcurl to write received data to a buffer.
 * @param contents Pointer to received data
 * @param size Size of each data element
 * @param nmemb Number of data elements
 * @param userp User pointer to response_buffer_t structure
 * @return Number of bytes written, or 0 on error
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


/**
 * Callback function for libcurl to process received HTTP headers.
 * @param buffer Pointer to header data
 * @param size Size of each data element
 * @param nitems Number of data elements
 * @param userdata User pointer to header_buffer_t structure
 * @return Number of bytes processed, or 0 on error
 */
static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata) {
  size_t realsize = size * nitems;
  header_buffer_t *headers = (header_buffer_t *)userdata;

  if (realsize <= 2) {
    return realsize;
  }

  if (headers->count >= headers->capacity) {
    size_t new_capacity = headers->capacity * 2;
    char **new_headers = realloc(headers->headers,
                                 new_capacity * sizeof(char *));
    if (!new_headers) {
      return 0;
    }
    headers->headers = new_headers;
    headers->capacity = new_capacity;
  }

  size_t len = realsize;
  while (len > 0 && (buffer[len - 1] == '\r' || buffer[len - 1] == '\n')) {
    len--;
  }

  if (len > 0) {
    headers->headers[headers->count] = malloc(len + 1);
    if (headers->headers[headers->count]) {
      memcpy(headers->headers[headers->count], buffer, len);
      headers->headers[headers->count][len] = '\0';
      headers->count++;
    }
  }

  return realsize;
}


/**
 * Frees memory allocated for header buffer and all stored headers.
 * @param headers Pointer to header_buffer_t structure to free
 */
static void free_header_buffer(header_buffer_t *headers) {
  for (size_t i = 0; i < headers->count; i++) {
    free(headers->headers[i]);
  }
  free(headers->headers);
}


/**
 * Prints a string as properly escaped JSON.
 * @param out Output stream
 * @param str String to print with JSON escaping
 */
static void print_json_string(FILE *out, const char *str) {
  fputc('"', out);
  for (const char *p = str; *p; p++) {
    switch (*p) {
      case '"':  fputs("\\\"", out); break;
      case '\\': fputs("\\\\", out); break;
      case '\b': fputs("\\b", out); break;
      case '\f': fputs("\\f", out); break;
      case '\n': fputs("\\n", out); break;
      case '\r': fputs("\\r", out); break;
      case '\t': fputs("\\t", out); break;
      default:
        if ((unsigned char)*p < 0x20) {
          fprintf(out, "\\u%04x", (unsigned char)*p);
        } else {
          fputc(*p, out);
        }
        break;
    }
  }
  fputc('"', out);
}


/**
 * Progress callback to check for interruption during transfer.
 * @param clientp Client data pointer (unused)
 * @param dltotal Total bytes to download
 * @param dlnow Bytes downloaded so far
 * @param ultotal Total bytes to upload
 * @param ulnow Bytes uploaded so far
 * @return Non-zero to abort the transfer, 0 to continue
 */
static int progress_callback(void *clientp, curl_off_t dltotal,
                             curl_off_t dlnow, curl_off_t ultotal,
                             curl_off_t ulnow) {
  (void)clientp;
  (void)dltotal;
  (void)dlnow;
  (void)ultotal;
  (void)ulnow;

  if (jshell_is_interrupted()) {
    return 1;  /* Abort transfer */
  }
  return 0;
}


/**
 * Main execution function for the http-post command.
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status (0 on success, non-zero on error)
 */
static int http_post_run(int argc, char **argv) {
  http_post_args_t args;
  build_http_post_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    http_post_print_usage(stdout);
    cleanup_http_post_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "http-post");
    fprintf(stderr, "Try 'http-post --help' for more information.\n");
    cleanup_http_post_argtable(&args);
    return 1;
  }

  const char *url = args.url->sval[0];
  const char *post_data = args.data->count > 0 ? args.data->sval[0] : "";
  int json_output = args.json_output->count > 0;

  CURL *curl = curl_easy_init();
  if (!curl) {
    if (json_output) {
      printf("{\"status\":\"error\","
             "\"message\":\"Failed to initialize curl\"}\n");
    } else {
      fprintf(stderr, "http-post: failed to initialize curl\n");
    }
    cleanup_http_post_argtable(&args);
    return 1;
  }

  response_buffer_t response = {
    .data = malloc(4096),
    .size = 0,
    .capacity = 4096
  };
  if (!response.data) {
    curl_easy_cleanup(curl);
    cleanup_http_post_argtable(&args);
    return 1;
  }
  response.data[0] = '\0';

  header_buffer_t resp_headers = {
    .headers = malloc(32 * sizeof(char *)),
    .count = 0,
    .capacity = 32
  };
  if (!resp_headers.headers) {
    free(response.data);
    curl_easy_cleanup(curl);
    cleanup_http_post_argtable(&args);
    return 1;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp_headers);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "jbox-http-post/1.0");

  /* Enable progress callback for signal interruption */
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);

  struct curl_slist *req_headers = NULL;
  for (int i = 0; i < args.headers->count; i++) {
    req_headers = curl_slist_append(req_headers, args.headers->sval[i]);
  }
  if (req_headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);
  }

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  char *content_type = NULL;
  curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);

  int ret = 0;

  if (res == CURLE_ABORTED_BY_CALLBACK) {
    /* Transfer was interrupted by signal */
    if (json_output) {
      printf("{\"status\":\"interrupted\",\"message\":\"Transfer interrupted\"}\n");
    } else {
      fprintf(stderr, "http-post: transfer interrupted\n");
    }
    ret = 130;  /* 128 + SIGINT(2) */
  } else if (res != CURLE_OK) {
    if (json_output) {
      printf("{\"status\":\"error\",\"code\":%d,\"message\":", (int)res);
      print_json_string(stdout, curl_easy_strerror(res));
      printf("}\n");
    } else {
      fprintf(stderr, "http-post: %s\n", curl_easy_strerror(res));
    }
    ret = 1;
  } else {
    if (json_output) {
      printf("{\"status\":\"ok\",\"http_code\":%ld", http_code);

      if (content_type) {
        printf(",\"content_type\":");
        print_json_string(stdout, content_type);
      }

      printf(",\"headers\":{");
      int first_header = 1;
      for (size_t i = 1; i < resp_headers.count; i++) {
        char *colon = strchr(resp_headers.headers[i], ':');
        if (colon) {
          if (!first_header) printf(",");
          first_header = 0;

          *colon = '\0';
          char *value = colon + 1;
          while (*value == ' ') value++;

          print_json_string(stdout, resp_headers.headers[i]);
          printf(":");
          print_json_string(stdout, value);

          *colon = ':';
        }
      }
      printf("}");

      printf(",\"body\":");
      print_json_string(stdout, response.data);
      printf("}\n");
    } else {
      printf("%s", response.data);
    }

    if (http_code >= 400) {
      ret = 1;
    }
  }

  if (req_headers) {
    curl_slist_free_all(req_headers);
  }
  free_header_buffer(&resp_headers);
  free(response.data);
  curl_easy_cleanup(curl);
  cleanup_http_post_argtable(&args);

  return ret;
}


/**
 * Command specification for http-post.
 */
const jshell_cmd_spec_t cmd_http_post_spec = {
  .name = "http-post",
  .summary = "send HTTP POST request to a URL",
  .long_help = "Send HTTP POST request to a URL. "
               "Supports custom headers, request body, and JSON output format.",
  .type = CMD_BUILTIN,
  .run = http_post_run,
  .print_usage = http_post_print_usage
};


/**
 * Registers the http-post command with the shell command registry.
 */
void jshell_register_http_post_command(void) {
  jshell_register_command(&cmd_http_post_spec);
}
