#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "jshell_ai_api.h"
#include "jshell_signals.h"


#define ANTHROPIC_API_URL "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION "2023-06-01"


typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} ResponseBuffer;


static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  ResponseBuffer *buf = (ResponseBuffer *)userp;

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


static int progress_callback(void *clientp, curl_off_t dltotal,
                             curl_off_t dlnow, curl_off_t ultotal,
                             curl_off_t ulnow) {
  (void)clientp;
  (void)dltotal;
  (void)dlnow;
  (void)ultotal;
  (void)ulnow;

  if (jshell_is_interrupted()) {
    return 1;
  }
  return 0;
}


/**
 * Escape a string for use in JSON.
 * Returns a newly allocated string that must be freed.
 */
static char *json_escape_string(const char *str) {
  if (str == NULL) {
    return strdup("");
  }

  /* Calculate required size */
  size_t len = 0;
  for (const char *p = str; *p; p++) {
    switch (*p) {
      case '"':
      case '\\':
      case '\b':
      case '\f':
      case '\n':
      case '\r':
      case '\t':
        len += 2;
        break;
      default:
        if ((unsigned char)*p < 0x20) {
          len += 6;  /* \uXXXX */
        } else {
          len += 1;
        }
        break;
    }
  }

  char *result = malloc(len + 1);
  if (!result) {
    return NULL;
  }

  char *out = result;
  for (const char *p = str; *p; p++) {
    switch (*p) {
      case '"':  *out++ = '\\'; *out++ = '"'; break;
      case '\\': *out++ = '\\'; *out++ = '\\'; break;
      case '\b': *out++ = '\\'; *out++ = 'b'; break;
      case '\f': *out++ = '\\'; *out++ = 'f'; break;
      case '\n': *out++ = '\\'; *out++ = 'n'; break;
      case '\r': *out++ = '\\'; *out++ = 'r'; break;
      case '\t': *out++ = '\\'; *out++ = 't'; break;
      default:
        if ((unsigned char)*p < 0x20) {
          out += sprintf(out, "\\u%04x", (unsigned char)*p);
        } else {
          *out++ = *p;
        }
        break;
    }
  }
  *out = '\0';

  return result;
}


/**
 * Build JSON request body for Anthropic API.
 * Returns a newly allocated string that must be freed.
 */
static char *build_request_json(const char *model, const char *system_prompt,
                                const char *user_message, int max_tokens) {
  char *escaped_system = json_escape_string(system_prompt);
  char *escaped_user = json_escape_string(user_message);

  if (!escaped_system || !escaped_user) {
    free(escaped_system);
    free(escaped_user);
    return NULL;
  }

  /* Calculate buffer size */
  size_t size = strlen(model) + strlen(escaped_system) + strlen(escaped_user)
                + 256;
  char *json = malloc(size);
  if (!json) {
    free(escaped_system);
    free(escaped_user);
    return NULL;
  }

  snprintf(json, size,
    "{"
      "\"model\":\"%s\","
      "\"max_tokens\":%d,"
      "\"system\":\"%s\","
      "\"messages\":["
        "{\"role\":\"user\",\"content\":\"%s\"}"
      "]"
    "}",
    model, max_tokens, escaped_system, escaped_user);

  free(escaped_system);
  free(escaped_user);

  return json;
}


/**
 * Extract content text from Anthropic API response.
 * Returns a newly allocated string that must be freed, or NULL on error.
 */
static char *extract_response_content(const char *json_response) {
  /*
   * Response format:
   * {
   *   "content": [
   *     {
   *       "type": "text",
   *       "text": "..."
   *     }
   *   ],
   *   ...
   * }
   */

  /* Find "content" array */
  const char *content_key = "\"content\"";
  const char *content_start = strstr(json_response, content_key);
  if (!content_start) {
    return NULL;
  }

  /* Find "text" key within content */
  const char *text_key = "\"text\"";
  const char *text_start = strstr(content_start, text_key);
  if (!text_start) {
    return NULL;
  }

  /* Skip to the value */
  text_start += strlen(text_key);
  while (*text_start && (*text_start == ':' || *text_start == ' '
                         || *text_start == '\t' || *text_start == '\n')) {
    text_start++;
  }

  if (*text_start != '"') {
    return NULL;
  }
  text_start++;  /* Skip opening quote */

  /* Find end of string, handling escapes */
  const char *text_end = text_start;
  while (*text_end && *text_end != '"') {
    if (*text_end == '\\' && *(text_end + 1)) {
      text_end += 2;  /* Skip escaped character */
    } else {
      text_end++;
    }
  }

  if (*text_end != '"') {
    return NULL;
  }

  /* Allocate and unescape the content */
  size_t escaped_len = text_end - text_start;
  char *result = malloc(escaped_len + 1);
  if (!result) {
    return NULL;
  }

  /* Unescape JSON string */
  char *out = result;
  const char *in = text_start;
  while (in < text_end) {
    if (*in == '\\' && (in + 1) < text_end) {
      in++;
      switch (*in) {
        case '"':  *out++ = '"'; break;
        case '\\': *out++ = '\\'; break;
        case 'b':  *out++ = '\b'; break;
        case 'f':  *out++ = '\f'; break;
        case 'n':  *out++ = '\n'; break;
        case 'r':  *out++ = '\r'; break;
        case 't':  *out++ = '\t'; break;
        case 'u':
          /* Handle \uXXXX - just copy basic ASCII range */
          if ((in + 4) < text_end) {
            unsigned int codepoint = 0;
            if (sscanf(in + 1, "%4x", &codepoint) == 1) {
              if (codepoint < 128) {
                *out++ = (char)codepoint;
              } else {
                /* For non-ASCII, just use placeholder */
                *out++ = '?';
              }
              in += 4;
            }
          }
          break;
        default:
          *out++ = *in;
          break;
      }
      in++;
    } else {
      *out++ = *in++;
    }
  }
  *out = '\0';

  return result;
}


/**
 * Extract error message from Anthropic API error response.
 * Returns a newly allocated string that must be freed, or NULL.
 */
static char *extract_error_message(const char *json_response) {
  /*
   * Error format:
   * {
   *   "error": {
   *     "type": "...",
   *     "message": "..."
   *   }
   * }
   */

  const char *error_key = "\"error\"";
  const char *error_start = strstr(json_response, error_key);
  if (!error_start) {
    return NULL;
  }

  const char *message_key = "\"message\"";
  const char *message_start = strstr(error_start, message_key);
  if (!message_start) {
    return NULL;
  }

  /* Skip to the value */
  message_start += strlen(message_key);
  while (*message_start && (*message_start == ':' || *message_start == ' '
                            || *message_start == '\t'
                            || *message_start == '\n')) {
    message_start++;
  }

  if (*message_start != '"') {
    return NULL;
  }
  message_start++;

  /* Find end of string */
  const char *message_end = message_start;
  while (*message_end && *message_end != '"') {
    if (*message_end == '\\' && *(message_end + 1)) {
      message_end += 2;
    } else {
      message_end++;
    }
  }

  size_t len = message_end - message_start;
  char *result = malloc(len + 1);
  if (!result) {
    return NULL;
  }

  memcpy(result, message_start, len);
  result[len] = '\0';

  return result;
}


AnthropicResponse jshell_anthropic_request(
    const char *api_key,
    const char *model,
    const char *system_prompt,
    const char *user_message,
    int max_tokens) {

  AnthropicResponse response = {
    .content = NULL,
    .success = 0,
    .error = NULL
  };

  if (!api_key || !model || !user_message) {
    response.error = strdup("Missing required parameters");
    return response;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    response.error = strdup("Failed to initialize curl");
    return response;
  }

  /* Build request JSON */
  char *request_json = build_request_json(model, system_prompt ? system_prompt
                                          : "", user_message, max_tokens);
  if (!request_json) {
    response.error = strdup("Failed to build request JSON");
    curl_easy_cleanup(curl);
    return response;
  }

  /* Prepare response buffer */
  ResponseBuffer resp_buf = {
    .data = malloc(4096),
    .size = 0,
    .capacity = 4096
  };
  if (!resp_buf.data) {
    response.error = strdup("Failed to allocate response buffer");
    free(request_json);
    curl_easy_cleanup(curl);
    return response;
  }
  resp_buf.data[0] = '\0';

  /* Set up headers */
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  char auth_header[256];
  snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key);
  headers = curl_slist_append(headers, auth_header);

  char version_header[64];
  snprintf(version_header, sizeof(version_header),
           "anthropic-version: %s", ANTHROPIC_VERSION);
  headers = curl_slist_append(headers, version_header);

  /* Configure curl */
  curl_easy_setopt(curl, CURLOPT_URL, ANTHROPIC_API_URL);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buf);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "jshell-ai/1.0");

  /* Enable progress callback for signal interruption */
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);

  /* Perform request */
  CURLcode res = curl_easy_perform(curl);

  if (res == CURLE_ABORTED_BY_CALLBACK) {
    response.error = strdup("Request interrupted");
  } else if (res != CURLE_OK) {
    response.error = strdup(curl_easy_strerror(res));
  } else {
    /* Check HTTP response code */
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == 200) {
      /* Extract content from response */
      response.content = extract_response_content(resp_buf.data);
      if (response.content) {
        response.success = 1;
      } else {
        response.error = strdup("Failed to parse API response");
      }
    } else {
      /* Extract error message */
      char *api_error = extract_error_message(resp_buf.data);
      if (api_error) {
        response.error = api_error;
      } else {
        char error_buf[128];
        snprintf(error_buf, sizeof(error_buf),
                 "API error (HTTP %ld)", http_code);
        response.error = strdup(error_buf);
      }
    }
  }

  /* Cleanup */
  curl_slist_free_all(headers);
  free(resp_buf.data);
  free(request_json);
  curl_easy_cleanup(curl);

  return response;
}


void jshell_free_anthropic_response(AnthropicResponse *resp) {
  if (resp) {
    free(resp->content);
    free(resp->error);
    resp->content = NULL;
    resp->error = NULL;
    resp->success = 0;
  }
}
