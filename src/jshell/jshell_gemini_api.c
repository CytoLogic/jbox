/**
 * @file jshell_gemini_api.c
 * @brief HTTP client for Google Gemini API
 *
 * Provides low-level integration with the Google Gemini API using libcurl.
 * Handles JSON request/response formatting, HTTP communication, error
 * handling, and user feedback (spinner) during API calls.
 *
 * Features:
 * - JSON request building with proper escaping
 * - Response parsing and content extraction
 * - Visual spinner feedback during API calls
 * - Interrupt handling (Ctrl+C support)
 * - Comprehensive error reporting
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "jshell_gemini_api.h"
#include "jshell_signals.h"


/** Base URL for Gemini API endpoints */
#define GEMINI_API_URL_BASE \
  "https://generativelanguage.googleapis.com/v1beta/models/"


/** Buffer for accumulating HTTP response data */
typedef struct {
  char *data;         /**< Response data buffer */
  size_t size;        /**< Current data size */
  size_t capacity;    /**< Buffer capacity */
} ResponseBuffer;


/**
 * Callback function for writing HTTP response data.
 *
 * Called by libcurl when response data is received. Accumulates data
 * into a ResponseBuffer, automatically growing the buffer as needed.
 *
 * @param contents Pointer to received data
 * @param size Size of each data element
 * @param nmemb Number of data elements
 * @param userp User-provided pointer (ResponseBuffer*)
 * @return Number of bytes processed, or 0 on error
 */
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


/** Spinner animation frames */
static const char *SPINNER_FRAMES[] = {"|", "/", "-", "\\"};

/** Number of spinner frames */
static const int SPINNER_FRAME_COUNT = 4;

/** Current spinner frame index */
static int g_spinner_frame = 0;

/** Whether spinner is currently active */
static int g_spinner_active = 0;

/** ANSI escape code for green text */
#define GREEN "\033[32m"

/** ANSI escape code to reset text formatting */
#define RESET "\033[0m"


/**
 * Start the spinner animation.
 *
 * Initializes spinner state and displays the first frame.
 */
static void spinner_start(void) {
  g_spinner_frame = 0;
  g_spinner_active = 1;
  fprintf(stderr, "\r" GREEN "%s" RESET, SPINNER_FRAMES[0]);
  fflush(stderr);
}


/**
 * Stop the spinner animation and clear it from the terminal.
 */
static void spinner_stop(void) {
  if (g_spinner_active) {
    fprintf(stderr, "\r \r");  /* Clear the spinner */
    fflush(stderr);
    g_spinner_active = 0;
  }
}


/**
 * Progress callback for libcurl.
 *
 * Called periodically during the HTTP request to:
 * - Check for user interrupts (Ctrl+C)
 * - Update the spinner animation
 *
 * @param clientp User data (unused)
 * @param dltotal Total bytes to download (unused)
 * @param dlnow Bytes downloaded so far (unused)
 * @param ultotal Total bytes to upload (unused)
 * @param ulnow Bytes uploaded so far (unused)
 * @return 0 to continue, 1 to abort the request
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
    spinner_stop();
    return 1;
  }

  /* Update spinner */
  if (g_spinner_active) {
    g_spinner_frame = (g_spinner_frame + 1) % SPINNER_FRAME_COUNT;
    fprintf(stderr, "\r" GREEN "%s" RESET, SPINNER_FRAMES[g_spinner_frame]);
    fflush(stderr);
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
 * Build JSON request body for Gemini API.
 *
 * Constructs a properly formatted JSON request with escaped strings for
 * the Gemini API's generateContent endpoint.
 *
 * Request format:
 * {
 *   "contents": [
 *     {"role": "user", "parts": [{"text": "..."}]}
 *   ],
 *   "systemInstruction": {"parts": [{"text": "..."}]},
 *   "generationConfig": {"maxOutputTokens": N}
 * }
 *
 * @param system_prompt System prompt to set context (may be NULL or empty)
 * @param user_message User's message content
 * @param max_tokens Maximum output tokens to generate
 * @return Newly allocated JSON string, or NULL on allocation failure.
 *         Caller must free the returned string.
 */
static char *build_request_json(const char *system_prompt,
                                const char *user_message, int max_tokens) {
  char *escaped_system = json_escape_string(system_prompt);
  char *escaped_user = json_escape_string(user_message);

  if (!escaped_system || !escaped_user) {
    free(escaped_system);
    free(escaped_user);
    return NULL;
  }

  /* Calculate buffer size */
  size_t size = strlen(escaped_system) + strlen(escaped_user) + 512;
  char *json = malloc(size);
  if (!json) {
    free(escaped_system);
    free(escaped_user);
    return NULL;
  }

  snprintf(json, size,
    "{"
      "\"contents\":["
        "{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]}"
      "],"
      "\"systemInstruction\":{\"parts\":[{\"text\":\"%s\"}]},"
      "\"generationConfig\":{\"maxOutputTokens\":%d}"
    "}",
    escaped_user, escaped_system, max_tokens);

  free(escaped_system);
  free(escaped_user);

  return json;
}


/**
 * Build the full API URL for the Gemini endpoint.
 *
 * Constructs the complete URL by combining the base URL, model name,
 * and the generateContent endpoint.
 *
 * @param model Model identifier (e.g., "gemini-2.5-flash")
 * @return Newly allocated URL string, or NULL on allocation failure.
 *         Caller must free the returned string.
 */
static char *build_api_url(const char *model) {
  /* URL format: BASE + model + ":generateContent" */
  size_t size = strlen(GEMINI_API_URL_BASE) + strlen(model) + 20;
  char *url = malloc(size);
  if (!url) {
    return NULL;
  }

  snprintf(url, size, "%s%s:generateContent", GEMINI_API_URL_BASE, model);

  return url;
}


/**
 * Extract content text from a successful Gemini API response.
 *
 * Parses the JSON response to extract the generated text from the
 * candidates[0].content.parts[0].text field. Handles JSON unescaping
 * to convert escape sequences back to their original characters.
 *
 * Expected response format:
 * {
 *   "candidates": [
 *     {
 *       "content": {
 *         "parts": [{"text": "..."}],
 *         "role": "model"
 *       }
 *     }
 *   ]
 * }
 *
 * @param json_response Complete JSON response from the API
 * @return Newly allocated string with extracted and unescaped content,
 *         or NULL if parsing fails. Caller must free the returned string.
 */
static char *extract_response_content(const char *json_response) {
  /* Find "candidates" array */
  const char *candidates_key = "\"candidates\"";
  const char *candidates_start = strstr(json_response, candidates_key);
  if (!candidates_start) {
    return NULL;
  }

  /* Find "parts" within candidates */
  const char *parts_key = "\"parts\"";
  const char *parts_start = strstr(candidates_start, parts_key);
  if (!parts_start) {
    return NULL;
  }

  /* Find "text" key within parts */
  const char *text_key = "\"text\"";
  const char *text_start = strstr(parts_start, text_key);
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
 * Extract error message from a Gemini API error response.
 *
 * Parses the JSON error response to extract the error message from
 * the error.message field. Does not unescape the message string.
 *
 * Error response format:
 * {
 *   "error": {
 *     "code": 400,
 *     "message": "...",
 *     "status": "..."
 *   }
 * }
 *
 * @param json_response Complete JSON error response from the API
 * @return Newly allocated string with error message, or NULL if parsing fails.
 *         Caller must free the returned string.
 */
static char *extract_error_message(const char *json_response) {
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


/**
 * Send a request to the Google Gemini API.
 *
 * Makes an HTTPS POST request to the Gemini API's generateContent endpoint
 * with the specified parameters. Displays a spinner during the request and
 * supports interrupt handling (Ctrl+C).
 *
 * The function performs the following steps:
 * 1. Validates input parameters
 * 2. Initializes libcurl
 * 3. Builds the API URL and request JSON
 * 4. Configures HTTP headers with API key
 * 5. Performs the request with spinner feedback
 * 6. Parses the response (success or error)
 * 7. Cleans up all resources
 *
 * @param api_key Google API key for authentication (required)
 * @param model Model identifier (e.g., "gemini-2.5-flash") (required)
 * @param system_prompt System prompt to set context (may be NULL or empty)
 * @param user_message User's message content (required)
 * @param max_tokens Maximum tokens in response
 * @return GeminiResponse containing either content (on success) or error.
 *         Caller must call jshell_free_gemini_response() to free memory.
 */
GeminiResponse jshell_gemini_request(
    const char *api_key,
    const char *model,
    const char *system_prompt,
    const char *user_message,
    int max_tokens) {

  GeminiResponse response = {
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

  /* Build API URL */
  char *api_url = build_api_url(model);
  if (!api_url) {
    response.error = strdup("Failed to build API URL");
    curl_easy_cleanup(curl);
    return response;
  }

  /* Build request JSON */
  char *request_json = build_request_json(system_prompt ? system_prompt : "",
                                          user_message, max_tokens);
  if (!request_json) {
    response.error = strdup("Failed to build request JSON");
    free(api_url);
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
    free(api_url);
    curl_easy_cleanup(curl);
    return response;
  }
  resp_buf.data[0] = '\0';

  /* Set up headers */
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  /* Add API key header */
  char api_key_header[256];
  snprintf(api_key_header, sizeof(api_key_header), "X-goog-api-key: %s",
           api_key);
  headers = curl_slist_append(headers, api_key_header);

  /* Configure curl */
  curl_easy_setopt(curl, CURLOPT_URL, api_url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buf);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "jshell-ai/1.0");

  /* Enable progress callback for spinner and signal interruption */
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);

  /* Start spinner and perform request */
  spinner_start();
  CURLcode res = curl_easy_perform(curl);
  spinner_stop();

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
  free(api_url);
  curl_easy_cleanup(curl);

  return response;
}


/**
 * Free memory allocated in a GeminiResponse.
 *
 * Frees the content and error strings and resets all fields to their
 * initial state. Safe to call on a response that has already been freed
 * or was never populated.
 *
 * @param resp Pointer to GeminiResponse to free (may be NULL)
 */
void jshell_free_gemini_response(GeminiResponse *resp) {
  if (resp) {
    free(resp->content);
    free(resp->error);
    resp->content = NULL;
    resp->error = NULL;
    resp->success = 0;
  }
}
