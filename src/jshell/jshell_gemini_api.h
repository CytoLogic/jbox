#ifndef JSHELL_GEMINI_API_H
#define JSHELL_GEMINI_API_H

/**
 * Google Gemini API client for jshell
 *
 * Low-level HTTP client for communicating with the Gemini API.
 */

/**
 * Response from a Gemini API request.
 */
typedef struct {
  char *content;      /* Response content text (caller must free) */
  int success;        /* 1 if successful, 0 if error */
  char *error;        /* Error message if failed (caller must free) */
} GeminiResponse;

/**
 * Send a request to the Google Gemini API.
 *
 * @param api_key       Google API key
 * @param model         Model ID (e.g., "gemini-2.5-flash")
 * @param system_prompt System prompt to set context
 * @param user_message  User message content
 * @param max_tokens    Maximum tokens in response
 *
 * @return GeminiResponse with content or error information.
 *         Caller must call jshell_free_gemini_response() to free memory.
 */
GeminiResponse jshell_gemini_request(
  const char *api_key,
  const char *model,
  const char *system_prompt,
  const char *user_message,
  int max_tokens
);

/**
 * Free memory allocated in a GeminiResponse.
 */
void jshell_free_gemini_response(GeminiResponse *resp);

#endif /* JSHELL_GEMINI_API_H */
