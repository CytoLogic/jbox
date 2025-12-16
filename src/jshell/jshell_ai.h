#ifndef JSHELL_AI_H
#define JSHELL_AI_H

/**
 * AI module for jshell
 *
 * Provides integration with Anthropic's Claude API for AI-assisted
 * shell operations:
 * - @query: Simple chat query
 * - @!query: AI-generated command execution
 */

/**
 * Initialize AI context.
 * Must be called after jshell_load_env_file() so ANTHROPIC_API_KEY is available.
 * Returns 0 on success, -1 if API key is not set.
 */
int jshell_ai_init(void);

/**
 * Cleanup AI context and free resources.
 */
void jshell_ai_cleanup(void);

/**
 * Check if AI functionality is available.
 * Returns 1 if API key is set and AI is ready, 0 otherwise.
 */
int jshell_ai_available(void);

/**
 * Send a simple chat query to the AI.
 * Returns the AI response as a newly allocated string (caller must free),
 * or NULL on error.
 */
char *jshell_ai_chat(const char *query);

/**
 * Send an execute query to the AI.
 * The AI will generate a valid jshell command based on the query.
 * Returns the command as a newly allocated string (caller must free),
 * or NULL on error.
 */
char *jshell_ai_execute_query(const char *query);

#endif /* JSHELL_AI_H */
