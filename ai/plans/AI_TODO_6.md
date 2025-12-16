# AI Integration Implementation Plan

## Status: COMPLETED

## Overview

Add AI functionality to jshell using the Anthropic API. The shell grammar already defines AI input tokens (`@query` for chat, `@!query` for execute), and the AST interpreter has placeholder functions that need to be implemented.

### Grammar (Already Defined)

From `src/shell-grammar/Grammar.cf`:
```bnf
token AIQueryToken ('@' ( char - ('!') ) (char)* ) ;
token AIExecToken (('@' '!' ) ( char )* ) ;

AIChatJob. Job ::= AIQueryToken ;
AIExecJob. Job ::= AIExecToken ;
```

### Existing Placeholder Functions

From `src/ast/jshell_ast_interpreter.c`:
```c
void visitAIQueryToken(AIQueryToken p) {
  DPRINT("visiting AIQueryToken: %s", p);
  printf("AI Query: %s\n", p);  // Currently just prints
}

void visitAIExecToken(AIExecToken p) {
  DPRINT("visiting AIExecToken: %s", p);
  printf("AI Exec: %s\n", p);   // Currently just prints
}
```

### Behavior Specification

1. **`@query`** - Simple AI chat query
   - Send `query` to Anthropic's Claude Haiku model
   - Display the response to the user
   - If query is empty (`@` alone), default to "Hi!"

2. **`@!query`** - AI execute query
   - Send `query` to Anthropic with shell context (available commands, grammar)
   - Receive a valid jshell command as response
   - Display the proposed command to user
   - Prompt user: `Execute? (Y/n):`
   - If `Y` or `y` or empty: parse and execute the command
   - If `n` or `N`: cancel

3. **API Key**
   - Read from `ANTHROPIC_API_KEY` environment variable
   - Loaded via `~/.jshell/env` at shell startup

---

## Phase 1: AI Module Infrastructure

### 1.1 Create AI Module Header ✅
**File**: `src/jshell/jshell_ai.h`

- [x] Define AI context structure:
  ```c
  typedef struct {
    char *api_key;
    char *model;
    char *system_prompt;
    char *command_context;  // Help text for all commands
    char *grammar_context;  // Shell grammar
  } JShellAIContext;
  ```

- [x] Declare public functions:
  ```c
  // Initialize AI context (call at shell startup)
  int jshell_ai_init(void);

  // Cleanup AI context
  void jshell_ai_cleanup(void);

  // Check if AI is available (API key present)
  int jshell_ai_available(void);

  // Send a simple chat query, returns response (caller frees)
  char *jshell_ai_chat(const char *query);

  // Send an execute query, returns command string (caller frees)
  char *jshell_ai_execute_query(const char *query);
  ```

### 1.2 Create AI Module Implementation ✅
**File**: `src/jshell/jshell_ai.c`

- [x] Implement `jshell_ai_init()`:
  - [x] Read `ANTHROPIC_API_KEY` from environment
  - [x] Store in global AI context
  - [x] Set model to "claude-3-haiku-20240307"
  - [x] Build command context (help for all commands)
  - [x] Build grammar context (from Grammar.cf or hardcoded)
  - [x] Return 0 on success, -1 if no API key

- [x] Implement `jshell_ai_cleanup()`:
  - [x] Free all allocated strings in context

- [x] Implement `jshell_ai_available()`:
  - [x] Return 1 if API key is set, 0 otherwise

- [x] Implement helper function `build_command_context()`:
  - [x] Use `jshell_for_each_command()` to iterate all commands
  - [x] For each command, call `spec->print_usage()` to capture help
  - [x] Concatenate all help text into a single string
  - [x] Use a memory stream or buffer to capture `print_usage()` output

- [x] Implement helper function `build_grammar_context()`:
  - [x] Include the shell grammar as a string constant
  - [x] Or read from Grammar.cf at runtime

### 1.3 Create Anthropic API Client ✅
**File**: `src/jshell/jshell_ai_api.h` and `src/jshell/jshell_ai_api.c`

- [x] Implement `jshell_anthropic_request()`:
  ```c
  typedef struct {
    char *content;      // Response content
    int success;        // 1 if successful, 0 if error
    char *error;        // Error message if failed
  } AnthropicResponse;

  AnthropicResponse jshell_anthropic_request(
    const char *api_key,
    const char *model,
    const char *system_prompt,
    const char *user_message
  );

  void jshell_free_anthropic_response(AnthropicResponse *resp);
  ```

- [x] Use libcurl for HTTP POST to `https://api.anthropic.com/v1/messages`
- [x] Set required headers:
  - [x] `x-api-key: <api_key>`
  - [x] `anthropic-version: 2023-06-01`
  - [x] `content-type: application/json`

- [x] Build JSON request body:
  ```json
  {
    "model": "claude-3-haiku-20240307",
    "max_tokens": 1024,
    "system": "<system_prompt>",
    "messages": [
      {"role": "user", "content": "<user_message>"}
    ]
  }
  ```

- [x] Parse JSON response to extract content
- [x] Handle errors (network, API errors, rate limits)

### 1.4 Implement Chat Function ✅
**File**: `src/jshell/jshell_ai.c`

- [x] Implement `jshell_ai_chat()`:
  - [x] Check if AI is available
  - [x] Build simple system prompt: "You are a helpful assistant."
  - [x] Call `jshell_anthropic_request()`
  - [x] Return response content or error message

### 1.5 Implement Execute Query Function ✅
**File**: `src/jshell/jshell_ai.c`

- [x] Implement `jshell_ai_execute_query()`:
  - [x] Check if AI is available
  - [x] Build execute system prompt with context:
    ```
    You are a shell command assistant for jshell. Your task is to
    produce a single, valid jshell command to accomplish the user's
    request.

    IMPORTANT: Respond with ONLY the command, no explanations, no
    markdown, no code blocks. Just the raw command string.

    Available commands and their usage:
    <command_context>

    Shell grammar:
    <grammar_context>
    ```
  - [x] Call `jshell_anthropic_request()`
  - [x] Strip any whitespace from response
  - [x] Return command string

---

## Phase 2: AST Interpreter Integration ✅

### 2.1 Update AST Interpreter ✅
**File**: `src/ast/jshell_ast_interpreter.c`

- [x] Add include: `#include "jshell/jshell_ai.h"`

- [x] Implement `visitAIQueryToken()`:
  ```c
  void visitAIQueryToken(AIQueryToken p) {
    DPRINT("visiting AIQueryToken: %s", p);

    // Extract query (skip the '@' prefix)
    const char *query = p + 1;

    // Default to "Hi!" if empty
    if (query[0] == '\0') {
      query = "Hi!";
    }

    if (!jshell_ai_available()) {
      fprintf(stderr, "jshell: AI not available "
              "(ANTHROPIC_API_KEY not set)\n");
      return;
    }

    char *response = jshell_ai_chat(query);
    if (response != NULL) {
      printf("%s\n", response);
      free(response);
    }
  }
  ```

- [x] Implement `visitAIExecToken()`:
  ```c
  void visitAIExecToken(AIExecToken p) {
    DPRINT("visiting AIExecToken: %s", p);

    // Extract query (skip the '@!' prefix)
    const char *query = p + 2;

    if (!jshell_ai_available()) {
      fprintf(stderr, "jshell: AI not available "
              "(ANTHROPIC_API_KEY not set)\n");
      return;
    }

    char *command = jshell_ai_execute_query(query);
    if (command == NULL) {
      fprintf(stderr, "jshell: AI failed to generate command\n");
      return;
    }

    printf("Proposed command: %s\n", command);
    printf("Execute? (Y/n): ");
    fflush(stdout);

    char response[16];
    if (fgets(response, sizeof(response), stdin) == NULL) {
      free(command);
      return;
    }

    // Accept Y, y, or empty (default yes)
    char c = response[0];
    if (c == 'n' || c == 'N') {
      printf("Cancelled.\n");
      free(command);
      return;
    }

    // Parse and execute the command
    Input parse_tree = psInput(command);
    if (parse_tree == NULL) {
      fprintf(stderr, "jshell: AI generated invalid command\n");
      free(command);
      return;
    }

    interpretInput(parse_tree);
    free_Input(parse_tree);
    free(command);
  }
  ```

### 2.2 Update Shell Initialization ✅
**File**: `src/jshell/jshell.c`

- [x] Add include: `#include "jshell_ai.h"`

- [x] Call `jshell_ai_init()` after `jshell_load_env_file()`:
  ```c
  jshell_init_signals();
  jshell_init_path();
  jshell_load_env_file();
  jshell_ai_init();  // Initialize AI after env is loaded
  jshell_init_job_control();
  // ...
  ```

- [x] Note: `jshell_ai_init()` should be called after env file is loaded
  so that `ANTHROPIC_API_KEY` from `~/.jshell/env` is available

---

## Phase 3: Build System Updates ✅

### 3.1 Update Makefile ✅
**File**: `Makefile`

- [x] Add AI source files to `JSHELL_SRCS`:
  ```makefile
  JSHELL_SRCS += \
    src/jshell/jshell_ai.c \
    src/jshell/jshell_ai_api.c
  ```

- [x] Ensure libcurl is linked (already done for http-get/http-post)

### 3.2 Create AI Headers ✅
**Files**: `src/jshell/jshell_ai.h`, `src/jshell/jshell_ai_api.h`

- [x] Create header files as specified in Phase 1

---

## Phase 4: Context Building ✅

### 4.1 Capture Command Help Output ✅
**File**: `src/jshell/jshell_ai.c`

- [x] Create helper to capture `print_usage()` output:
  ```c
  static char *capture_command_help(const jshell_cmd_spec_t *spec) {
    // Use open_memstream to capture output
    char *buffer = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buffer, &size);

    if (stream == NULL) {
      return NULL;
    }

    if (spec->print_usage != NULL) {
      spec->print_usage(stream);
    } else {
      fprintf(stream, "%s - %s\n", spec->name, spec->summary);
    }

    fclose(stream);
    return buffer;
  }
  ```

- [x] Create callback for `jshell_for_each_command()`:
  ```c
  typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
  } HelpBuffer;

  static void collect_command_help(const jshell_cmd_spec_t *spec,
                                   void *userdata) {
    HelpBuffer *buf = (HelpBuffer *)userdata;
    char *help = capture_command_help(spec);
    if (help != NULL) {
      // Append to buffer with separator
      // ... buffer management code ...
      free(help);
    }
  }
  ```

### 4.2 Store Grammar Context ✅
**File**: `src/jshell/jshell_ai.c`

- [x] Include shell grammar as a string constant:
  ```c
  static const char *SHELL_GRAMMAR =
    "-- jshell grammar\n"
    "\n"
    "token AIQueryToken ('@' ( char - ('!') ) (char)* ) ;\n"
    "token AIExecToken (('@' '!' ) ( char )* ) ;\n"
    // ... rest of grammar ...
    ;
  ```

- [x] Or read from file at runtime if preferred

---

## Phase 5: JSON Handling ✅

### 5.1 JSON Request Building ✅
**File**: `src/jshell/jshell_ai_api.c`

- [x] Create helper to escape JSON strings:
  ```c
  static char *json_escape_string(const char *str) {
    // Escape special characters: \, ", \n, \r, \t, etc.
    // Similar to print_json_string in cmd_http_get.c
  }
  ```

- [x] Build request JSON:
  ```c
  static char *build_request_json(const char *model,
                                  const char *system_prompt,
                                  const char *user_message) {
    char *escaped_system = json_escape_string(system_prompt);
    char *escaped_user = json_escape_string(user_message);

    // Calculate buffer size and format JSON
    // ...

    free(escaped_system);
    free(escaped_user);
    return json_str;
  }
  ```

### 5.2 JSON Response Parsing ✅
**File**: `src/jshell/jshell_ai_api.c`

- [x] Parse Anthropic response to extract content:
  ```c
  static char *extract_response_content(const char *json_response) {
    // Find "content" array
    // Find first text block
    // Extract "text" value
    // Return as newly allocated string
  }
  ```

- [x] Handle error responses:
  ```c
  static int is_error_response(const char *json_response) {
    // Check for "error" key in response
  }

  static char *extract_error_message(const char *json_response) {
    // Extract error.message from response
  }
  ```

---

## Phase 6: Testing ✅

### 6.1 Manual Testing ✅
- [x] Set up `~/.jshell/env` with API key:
  ```
  ANTHROPIC_API_KEY=sk-ant-...
  ```

- [x] Test simple chat:
  ```
  (jsh)>@hello
  Hello! How can I help you today?

  (jsh)>@what is 2+2?
  4
  ```

- [x] Test execute query:
  ```
  (jsh)>@!list the current directory
  Proposed command: ls .
  Execute? (Y/n): y
  file1.txt  file2.txt  dir1/

  (jsh)>@!show the first 5 lines of README.md
  Proposed command: head -n 5 README.md
  Execute? (Y/n): n
  Cancelled.
  ```

- [x] Test empty query:
  ```
  (jsh)>@
  Hi! How can I help you today?
  ```

- [x] Test missing API key:
  ```
  (jsh)>@hello
  jshell: AI not available (ANTHROPIC_API_KEY not set)
  ```

### 6.2 Create Unit Tests (Optional)
**File**: `tests/jshell/test_ai.py`

- [x] Test API key detection
- [x] Test JSON escaping
- [x] Test request building
- [x] Test response parsing
- [x] Mock API responses for testing

---

## Implementation Notes

### Token Parsing

The tokens from the grammar include the prefix:
- `AIQueryToken` captures `@query` (includes the `@`)
- `AIExecToken` captures `@!query` (includes the `@!`)

When processing, skip the appropriate number of characters:
- For `@query`: skip 1 character (`p + 1`)
- For `@!query`: skip 2 characters (`p + 2`)

### API Considerations

1. **Rate Limits**: Anthropic has rate limits; handle 429 responses gracefully
2. **Timeouts**: Set reasonable timeouts for API calls (e.g., 30 seconds)
3. **Error Messages**: Display user-friendly error messages
4. **Model Selection**: Use Haiku for speed/cost; could add option for other models

### Security Considerations

1. **API Key Storage**: Environment variable is reasonable for personal use
2. **Command Injection**: The AI-generated command is parsed by the shell's
   parser, so it goes through normal safety checks
3. **User Confirmation**: The `@!` command requires explicit user confirmation

### Context Size

The Anthropic API has context limits. The command help text could be large.
Consider:
- Summarizing command help instead of full usage
- Only including commands relevant to the query
- Caching the context string

---

## File Reference

### New Files to Create

```
src/jshell/jshell_ai.h          # AI module header
src/jshell/jshell_ai.c          # AI module implementation
src/jshell/jshell_ai_api.h      # Anthropic API client header
src/jshell/jshell_ai_api.c      # Anthropic API client implementation
tests/jshell/test_ai.py         # AI module tests (optional)
```

### Files to Modify

```
src/ast/jshell_ast_interpreter.c  # Implement visitAIQueryToken, visitAIExecToken
src/jshell/jshell.c               # Call jshell_ai_init() at startup
Makefile                          # Add AI source files
```

---

## Checklist Summary

- [x] Phase 1: AI Module Infrastructure (5 tasks)
  - [x] 1.1 Create AI module header
  - [x] 1.2 Create AI module implementation
  - [x] 1.3 Create Anthropic API client
  - [x] 1.4 Implement chat function
  - [x] 1.5 Implement execute query function
- [x] Phase 2: AST Interpreter Integration (2 tasks)
  - [x] 2.1 Update AST interpreter
  - [x] 2.2 Update shell initialization
- [x] Phase 3: Build System Updates (2 tasks)
  - [x] 3.1 Update Makefile
  - [x] 3.2 Create AI headers
- [x] Phase 4: Context Building (2 tasks)
  - [x] 4.1 Capture command help output
  - [x] 4.2 Store grammar context
- [x] Phase 5: JSON Handling (2 tasks)
  - [x] 5.1 JSON request building
  - [x] 5.2 JSON response parsing
- [x] Phase 6: Testing (2 tasks)
  - [x] 6.1 Manual testing
  - [x] 6.2 Create unit tests (optional)

**Total: 15 major tasks - ALL COMPLETED**
