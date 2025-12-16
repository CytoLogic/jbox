/**
 * @file jshell_ai.c
 * @brief AI integration module for jshell using Google Gemini API
 *
 * Provides two main AI features:
 * - Chat queries (@query): conversational AI responses
 * - Execute queries (@!query): AI-generated shell commands
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jshell_ai.h"
#include "jshell_ai_context.h"
#include "jshell_gemini_api.h"
#include "utils/jbox_utils.h"


/** Default AI model for all requests */
#define AI_MODEL "gemini-2.5-flash"

/** Maximum tokens for chat responses */
#define AI_CHAT_MAX_TOKENS 1024

/** Maximum tokens for command generation responses */
#define AI_EXEC_MAX_TOKENS 512


/** System prompt for chat queries (no command context) */
static const char *CHAT_SYSTEM_PROMPT =
  "You are a helpful assistant running inside jshell, a custom Unix-like "
  "shell. Keep responses concise and focused.";


/** Internal AI context state */
typedef struct {
  char *api_key;      /**< Google API key from environment */
  int initialized;    /**< 1 if module is initialized, 0 otherwise */
} AIContext;

/** Global AI context singleton */
static AIContext g_ai_ctx = {
  .api_key = NULL,
  .initialized = 0
};


/**
 * Initialize the AI module.
 *
 * Loads the Google API key from the GOOGLE_API_KEY environment variable
 * and marks the module as initialized. Must be called before using any
 * AI features.
 *
 * @return 0 on success, -1 if API key is not set or allocation fails
 */
int jshell_ai_init(void) {
  if (g_ai_ctx.initialized) {
    return 0;
  }

  /* Get API key from environment */
  const char *api_key = getenv("GOOGLE_API_KEY");
  if (!api_key || api_key[0] == '\0') {
    DPRINT("GOOGLE_API_KEY not set, AI features disabled");
    return -1;
  }

  g_ai_ctx.api_key = strdup(api_key);
  if (!g_ai_ctx.api_key) {
    return -1;
  }

  g_ai_ctx.initialized = 1;
  DPRINT("AI module initialized");

  return 0;
}


/**
 * Clean up the AI module and free all resources.
 *
 * Frees the API key and resets the module to uninitialized state.
 * Safe to call multiple times.
 */
void jshell_ai_cleanup(void) {
  free(g_ai_ctx.api_key);
  g_ai_ctx.api_key = NULL;
  g_ai_ctx.initialized = 0;
}


/**
 * Check if AI functionality is currently available.
 *
 * @return 1 if the module is initialized and has a valid API key, 0 otherwise
 */
int jshell_ai_available(void) {
  return g_ai_ctx.initialized && g_ai_ctx.api_key != NULL;
}


/**
 * Send a chat query to the AI.
 *
 * This function handles simple conversational queries without command context.
 * The AI responds with helpful information, keeping responses concise.
 *
 * @param query User's chat message (NULL or empty defaults to "Hi!")
 * @return Newly allocated string with AI response or error message.
 *         Caller must free the returned string.
 */
char *jshell_ai_chat(const char *query) {
  if (!jshell_ai_available()) {
    return strdup("AI not available (GOOGLE_API_KEY not set)");
  }

  if (!query || query[0] == '\0') {
    query = "Hi!";
  }

  DPRINT("AI chat query: %s", query);
  DPRINT("AI chat system prompt: %s", CHAT_SYSTEM_PROMPT);

  GeminiResponse resp = jshell_gemini_request(
    g_ai_ctx.api_key,
    AI_MODEL,
    CHAT_SYSTEM_PROMPT,
    query,
    AI_CHAT_MAX_TOKENS
  );

  char *result = NULL;
  if (resp.success && resp.content) {
    result = strdup(resp.content);
  } else if (resp.error) {
    /* Return error message as result */
    size_t len = strlen(resp.error) + 32;
    result = malloc(len);
    if (result) {
      snprintf(result, len, "AI error: %s", resp.error);
    }
  } else {
    result = strdup("AI error: Unknown error");
  }

  jshell_free_gemini_response(&resp);
  return result;
}


/**
 * Generate a shell command from a natural language query.
 *
 * This function sends a query to the AI with full context about jshell's
 * grammar, available commands, and their usage. The AI generates a valid
 * shell command that can be executed directly.
 *
 * The system prompt includes:
 * - Execution instructions for the AI
 * - Shell grammar specification
 * - Available command documentation
 *
 * @param query Natural language description of desired command
 * @return Newly allocated string with generated command (trimmed of whitespace),
 *         or NULL if AI is unavailable, query is empty, or an error occurs.
 *         Caller must free the returned string.
 */
char *jshell_ai_execute_query(const char *query) {
  if (!jshell_ai_available()) {
    return NULL;
  }

  if (!query || query[0] == '\0') {
    return NULL;
  }

  /* Build system prompt from embedded context */
  size_t prompt_size = strlen(EXEC_INSTRUCTIONS)
                     + strlen(GRAMMAR_CONTEXT)
                     + strlen(CMD_CONTEXT)
                     + 128;  /* padding for separators */

  char *system_prompt = malloc(prompt_size);
  if (!system_prompt) {
    return NULL;
  }

  snprintf(system_prompt, prompt_size,
           "%s\n\n"
           "=== SHELL GRAMMAR ===\n%s\n\n"
           "=== AVAILABLE COMMANDS ===\n%s",
           EXEC_INSTRUCTIONS,
           GRAMMAR_CONTEXT,
           CMD_CONTEXT);

  DPRINT("AI exec query: %s", query);
  DPRINT("AI exec system prompt:\n%s", system_prompt);

  GeminiResponse resp = jshell_gemini_request(
    g_ai_ctx.api_key,
    AI_MODEL,
    system_prompt,
    query,
    AI_EXEC_MAX_TOKENS
  );

  free(system_prompt);

  char *result = NULL;
  if (resp.success && resp.content) {
    /* Strip whitespace from response */
    const char *start = resp.content;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n'
                      || *start == '\r')) {
      start++;
    }

    const char *end = start + strlen(start);
    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t'
                           || *(end - 1) == '\n' || *(end - 1) == '\r')) {
      end--;
    }

    size_t len = end - start;
    if (len > 0) {
      result = malloc(len + 1);
      if (result) {
        memcpy(result, start, len);
        result[len] = '\0';
      }
    }
  } else if (resp.error) {
    fprintf(stderr, "jshell: AI error: %s\n", resp.error);
  }

  jshell_free_gemini_response(&resp);
  return result;
}
