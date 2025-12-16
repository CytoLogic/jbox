#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jshell_ai.h"
#include "jshell_gemini_api.h"
#include "jshell_cmd_registry.h"
#include "utils/jbox_utils.h"


#define AI_MODEL "gemini-2.5-flash"
#define AI_CHAT_MAX_TOKENS 1024
#define AI_EXEC_MAX_TOKENS 512


/* Shell grammar for AI context */
static const char *SHELL_GRAMMAR =
  "-- jshell grammar\n"
  "\n"
  "separator nonempty Job \";\" ;\n"
  "separator nonempty ShellToken \" \" ;\n"
  "\n"
  "In. Input ::= [Job] ;\n"
  "\n"
  "AssigJob. Job ::= ShellToken \"=\" CommandLine ;\n"
  "AIChatJob. Job ::= AIQueryToken ;     -- @query\n"
  "AIExecJob. Job ::= AIExecToken ;      -- @!query\n"
  "FGJob. Job ::= CommandLine ;\n"
  "BGJob. Job ::= CommandLine \"&\" ;\n"
  "\n"
  "CmdLine. CommandLine ::= CommandPart OptionalInputRedirection "
    "OptionalOutputRedirection ;\n"
  "\n"
  "SnglCmd. CommandPart ::= CommandUnit ;\n"
  "PipeCmd. CommandPart ::= CommandUnit \"|\" CommandPart ;\n"
  "\n"
  "NoInRedir. OptionalInputRedirection ::= ;\n"
  "InRedir. OptionalInputRedirection ::= \"<\" ShellToken ;\n"
  "NoOutRedir. OptionalOutputRedirection ::= ;\n"
  "OutRedir. OptionalOutputRedirection ::= \">\" ShellToken ;\n"
  "\n"
  "CmdUnit. CommandUnit ::= [ShellToken] ;\n"
  "\n"
  "-- Tokens: double-quoted strings expand variables, "
    "single-quoted are literal\n"
  "-- Variables: $NAME or $? for last exit status\n"
  "-- Words: unquoted text (no pipes, redirects, etc.)\n";


/* System prompts */
static const char *CHAT_SYSTEM_PROMPT =
  "You are a helpful assistant running inside jshell, a custom Unix-like "
  "shell. Keep responses concise and focused. When discussing shell commands, "
  "prefer jshell's available commands.";

static const char *EXEC_SYSTEM_PROMPT_PREFIX =
  "You are a shell command assistant for jshell. Your task is to produce a "
  "single, valid jshell command to accomplish the user's request.\n"
  "\n"
  "IMPORTANT RULES:\n"
  "1. Respond with ONLY the command - no explanations, no markdown, no code "
  "blocks\n"
  "2. Just output the raw command string that can be executed directly\n"
  "3. Use only the available commands listed below\n"
  "4. Use proper shell syntax (pipes |, redirects < >, background &)\n"
  "\n"
  "Shell grammar:\n";


/* Global AI context */
typedef struct {
  char *api_key;
  char *command_context;
  int initialized;
} AIContext;

static AIContext g_ai_ctx = {
  .api_key = NULL,
  .command_context = NULL,
  .initialized = 0
};


/* Dynamic string buffer for building context */
typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} StringBuffer;


static void strbuf_init(StringBuffer *buf) {
  buf->data = malloc(4096);
  buf->size = 0;
  buf->capacity = 4096;
  if (buf->data) {
    buf->data[0] = '\0';
  }
}


static void strbuf_free(StringBuffer *buf) {
  free(buf->data);
  buf->data = NULL;
  buf->size = 0;
  buf->capacity = 0;
}


static void strbuf_append(StringBuffer *buf, const char *str) {
  if (!buf->data || !str) {
    return;
  }

  size_t len = strlen(str);
  if (buf->size + len >= buf->capacity) {
    size_t new_capacity = buf->capacity * 2;
    if (new_capacity < buf->size + len + 1) {
      new_capacity = buf->size + len + 1;
    }
    char *new_data = realloc(buf->data, new_capacity);
    if (!new_data) {
      return;
    }
    buf->data = new_data;
    buf->capacity = new_capacity;
  }

  memcpy(buf->data + buf->size, str, len);
  buf->size += len;
  buf->data[buf->size] = '\0';
}


/**
 * Callback to collect command help into a string buffer.
 */
static void collect_command_help(const jshell_cmd_spec_t *spec, void *userdata)
{
  StringBuffer *buf = (StringBuffer *)userdata;

  if (!spec || !spec->name) {
    return;
  }

  /* Add command name and summary */
  strbuf_append(buf, spec->name);
  if (spec->summary) {
    strbuf_append(buf, " - ");
    strbuf_append(buf, spec->summary);
  }
  strbuf_append(buf, "\n");

  /* Capture print_usage output if available */
  if (spec->print_usage) {
    char *usage_buf = NULL;
    size_t usage_size = 0;
    FILE *stream = open_memstream(&usage_buf, &usage_size);

    if (stream) {
      spec->print_usage(stream);
      fclose(stream);

      if (usage_buf && usage_size > 0) {
        strbuf_append(buf, usage_buf);
        strbuf_append(buf, "\n");
      }
      free(usage_buf);
    }
  }

  strbuf_append(buf, "\n");
}


/**
 * Build command context from all registered commands.
 */
static char *build_command_context(void) {
  StringBuffer buf;
  strbuf_init(&buf);

  if (!buf.data) {
    return NULL;
  }

  strbuf_append(&buf, "Available commands:\n\n");
  jshell_for_each_command(collect_command_help, &buf);

  char *result = buf.data;
  buf.data = NULL;  /* Transfer ownership */
  return result;
}


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

  /* Build command context */
  g_ai_ctx.command_context = build_command_context();

  g_ai_ctx.initialized = 1;
  DPRINT("AI module initialized");

  return 0;
}


void jshell_ai_cleanup(void) {
  free(g_ai_ctx.api_key);
  free(g_ai_ctx.command_context);
  g_ai_ctx.api_key = NULL;
  g_ai_ctx.command_context = NULL;
  g_ai_ctx.initialized = 0;
}


int jshell_ai_available(void) {
  return g_ai_ctx.initialized && g_ai_ctx.api_key != NULL;
}


char *jshell_ai_chat(const char *query) {
  if (!jshell_ai_available()) {
    return strdup("AI not available (GOOGLE_API_KEY not set)");
  }

  if (!query || query[0] == '\0') {
    query = "Hi!";
  }

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


char *jshell_ai_execute_query(const char *query) {
  if (!jshell_ai_available()) {
    return NULL;
  }

  if (!query || query[0] == '\0') {
    return NULL;
  }

  /* Build execute system prompt with context */
  StringBuffer prompt_buf;
  strbuf_init(&prompt_buf);

  if (!prompt_buf.data) {
    return NULL;
  }

  strbuf_append(&prompt_buf, EXEC_SYSTEM_PROMPT_PREFIX);
  strbuf_append(&prompt_buf, SHELL_GRAMMAR);
  strbuf_append(&prompt_buf, "\n\n");
  if (g_ai_ctx.command_context) {
    strbuf_append(&prompt_buf, g_ai_ctx.command_context);
  }

  GeminiResponse resp = jshell_gemini_request(
    g_ai_ctx.api_key,
    AI_MODEL,
    prompt_buf.data,
    query,
    AI_EXEC_MAX_TOKENS
  );

  strbuf_free(&prompt_buf);

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
