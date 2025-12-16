#ifndef JSHELL_AI_API_H
#define JSHELL_AI_API_H

/**
 * Anthropic API client for jshell
 *
 * Low-level HTTP client for communicating with the Anthropic Messages API.
 */

/**
 * Response from an Anthropic API request.
 */
typedef struct {
  char *content;      /* Response content text (caller must free) */
  int success;        /* 1 if successful, 0 if error */
  char *error;        /* Error message if failed (caller must free) */
} AnthropicResponse;

/**
 * Send a request to the Anthropic Messages API.
 *
 * @param api_key       Anthropic API key
 * @param model         Model ID (e.g., "claude-3-haiku-20240307")
 * @param system_prompt System prompt to set context
 * @param user_message  User message content
 * @param max_tokens    Maximum tokens in response
 *
 * @return AnthropicResponse with content or error information.
 *         Caller must call jshell_free_anthropic_response() to free memory.
 */
AnthropicResponse jshell_anthropic_request(
  const char *api_key,
  const char *model,
  const char *system_prompt,
  const char *user_message,
  int max_tokens
);

/**
 * Free memory allocated in an AnthropicResponse.
 */
void jshell_free_anthropic_response(AnthropicResponse *resp);

#endif /* JSHELL_AI_API_H */
