# AI Context Embedding Implementation Plan

## Status: COMPLETED

## Overview

Refactor the AI integration to use C23 `#embed` directive for compiling context files into the binary. This removes runtime context building and uses pre-generated context files for better consistency and performance.

### Goals

1. Embed context files at compile time using C23 `#embed` directive
2. For `AIExecJobs`: Pass EXEC_INSTRUCTIONS, CMD_CONTEXT, and GRAMMAR_CONTEXT to model
3. For `AIQueryJobs`: Pass only the query (no context)

### Context Files

Located in `src/jshell/ai_context/`:
- `exec_context.txt` - Instructions for the AI when generating commands
- `cmd_context.txt` - Help text for all available commands
- `grammar_context.txt` - Shell grammar specification

---

## Phase 1: Add Embedded Context Constants

### 1.1 Create Embedded Context Header
**File**: `src/jshell/jshell_ai_context.h`

- [x] Create new header file with embedded constants:
  ```c
  #ifndef JSHELL_AI_CONTEXT_H
  #define JSHELL_AI_CONTEXT_H

  /**
   * Embedded AI context constants.
   * These are compiled into the binary using C23 #embed directive.
   */

  /* Instructions for AI exec queries */
  extern const char EXEC_INSTRUCTIONS[];

  /* Command help text for all available commands */
  extern const char CMD_CONTEXT[];

  /* Shell grammar specification */
  extern const char GRAMMAR_CONTEXT[];

  #endif /* JSHELL_AI_CONTEXT_H */
  ```

### 1.2 Create Embedded Context Implementation
**File**: `src/jshell/jshell_ai_context.c`

- [x] Create implementation file with `#embed` directives:
  ```c
  #include "jshell_ai_context.h"

  /* Use C23 #embed to compile file contents into constants */

  const char EXEC_INSTRUCTIONS[] = {
    #embed "ai_context/exec_context.txt"
    , '\0'
  };

  const char CMD_CONTEXT[] = {
    #embed "ai_context/cmd_context.txt"
    , '\0'
  };

  const char GRAMMAR_CONTEXT[] = {
    #embed "ai_context/grammar_context.txt"
    , '\0'
  };
  ```

---

## Phase 2: Update AI Module

### 2.1 Remove Dynamic Context Building
**File**: `src/jshell/jshell_ai.c`

- [x] Remove the `SHELL_GRAMMAR` static constant (lines 17-47)
- [x] Remove the `EXEC_SYSTEM_PROMPT_PREFIX` static constant (lines 56-67)
- [x] Remove the `StringBuffer` typedef and related functions:
  - `strbuf_init()` (lines 92-99)
  - `strbuf_free()` (lines 102-107)
  - `strbuf_append()` (lines 110-132)
- [x] Remove `collect_command_help()` callback function (lines 138-173)
- [x] Remove `build_command_context()` function (lines 179-193)
- [x] Remove `command_context` field from `AIContext` struct
- [x] Remove `g_ai_ctx.command_context` allocation in `jshell_ai_init()`
- [x] Remove `g_ai_ctx.command_context` cleanup in `jshell_ai_cleanup()`

### 2.2 Add Embedded Context Include
**File**: `src/jshell/jshell_ai.c`

- [x] Add include for embedded context:
  ```c
  #include "jshell_ai_context.h"
  ```

### 2.3 Update jshell_ai_execute_query()
**File**: `src/jshell/jshell_ai.c`

- [x] Update to use embedded constants:
  ```c
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
                       + 64;  /* padding for separators */

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

    GeminiResponse resp = jshell_gemini_request(
      g_ai_ctx.api_key,
      AI_MODEL,
      system_prompt,
      query,
      AI_EXEC_MAX_TOKENS
    );

    free(system_prompt);

    /* ... rest of response handling ... */
  }
  ```

### 2.4 Update jshell_ai_chat()
**File**: `src/jshell/jshell_ai.c`

- [x] Verify chat function passes only query (no context):
  - Current implementation already does this correctly
  - Uses simple `CHAT_SYSTEM_PROMPT` without command/grammar context
  - Added DPRINT() calls for debugging

---

## Phase 3: Update Build System

### 3.1 Update Makefile
**File**: `Makefile`

- [x] Add new source file to `JSHELL_SRCS`:
  ```makefile
  JSHELL_SRCS += \
    src/jshell/jshell_ai_context.c
  ```

- [ ] Ensure C23 standard is used (should already be set via `-std=gnu23`)

- [ ] Add include path for `#embed` to find context files:
  ```makefile
  # The #embed directive searches relative to the source file location,
  # so no additional flags needed if context files are in ai_context/
  # subdirectory relative to jshell_ai_context.c
  ```

### 3.2 Verify Compiler Support
- [x] Confirm GCC version supports C23 `#embed`:
  - GCC 15+ has native `#embed` support
  - If using older GCC, may need fallback approach (see Notes)

---

## Phase 4: Simplify AIContext Structure

### 4.1 Remove Unused Fields
**File**: `src/jshell/jshell_ai.c`

- [x] Simplify `AIContext` struct:
  ```c
  /* Global AI context */
  typedef struct {
    char *api_key;
    int initialized;
  } AIContext;
  ```

### 4.2 Update Initialization
**File**: `src/jshell/jshell_ai.c`

- [x] Simplify `jshell_ai_init()`:
  ```c
  int jshell_ai_init(void) {
    if (g_ai_ctx.initialized) {
      return 0;
    }

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
  ```

### 4.3 Update Cleanup
**File**: `src/jshell/jshell_ai.c`

- [x] Simplify `jshell_ai_cleanup()`:
  ```c
  void jshell_ai_cleanup(void) {
    free(g_ai_ctx.api_key);
    g_ai_ctx.api_key = NULL;
    g_ai_ctx.initialized = 0;
  }
  ```

---

## Phase 5: Testing

### 5.1 Build and Compile Test
- [x] Run `make clean && make jbox DEBUG=1`
- [x] Verify no compilation errors with `#embed`
- [x] Check binary size increase is reasonable

### 5.2 Functional Testing
- [x] Test `@query` (AIQueryJobs):
  ```
  (jsh)>@hello
  # Should respond without command context
  ```

- [x] Test `@!query` (AIExecJobs):
  ```
  (jsh)>@!list files in current directory
  # Should generate: ls
  ```

- [x] Test complex exec query:
  ```
  (jsh)>@!show first 5 lines of README.md
  # Should generate: head -n 5 README.md
  ```

### 5.3 Context Verification
- [x] Verify exec queries receive proper context by testing commands that:
  - Use shell-specific syntax (pipes, redirects)
  - Reference multiple available commands
  - Require understanding of command options
  - Verified: embedded context found in binary (strings check)

---

## Implementation Notes

### C23 #embed Directive

The `#embed` directive is a C23 feature that embeds file contents directly into the binary at compile time:

```c
const char content[] = {
  #embed "path/to/file.txt"
  , '\0'  // Add null terminator for string usage
};
```

**Key points:**
- Path is relative to the source file location
- File is read at compile time, not runtime
- Contents become part of the binary's data section
- Null terminator must be added manually for string usage

### Fallback for Older Compilers

If GCC < 15 is used (no native `#embed` support), use xxd to generate include files:

```makefile
# Generate header from text file
src/jshell/ai_context/exec_context.inc: src/jshell/ai_context/exec_context.txt
	xxd -i < $< > $@
```

Then in C:
```c
const char EXEC_INSTRUCTIONS[] = {
  #include "ai_context/exec_context.inc"
  , '\0'
};
```

### Context File Maintenance

When commands are added/modified:
1. Update `cmd_context.txt` with new command help
2. Rebuild shell to embed updated context

Consider adding a script to regenerate `cmd_context.txt` from running commands:
```bash
for cmd in ls cat head ...; do
  echo "=== $cmd ===" >> cmd_context.txt
  bin/$cmd --help >> cmd_context.txt 2>&1
  echo "" >> cmd_context.txt
done
```

---

## File Reference

### New Files to Create

```
src/jshell/jshell_ai_context.h    # Embedded context header
src/jshell/jshell_ai_context.c    # Embedded context implementation
```

### Files to Modify

```
src/jshell/jshell_ai.c            # Remove dynamic context, use embedded
Makefile                          # Add new source file
```

### Existing Context Files (no changes)

```
src/jshell/ai_context/exec_context.txt     # Exec instructions
src/jshell/ai_context/cmd_context.txt      # Command help
src/jshell/ai_context/grammar_context.txt  # Shell grammar
```

---

## Checklist Summary

- [x] Phase 1: Add Embedded Context Constants (2 tasks)
  - [x] 1.1 Create embedded context header
  - [x] 1.2 Create embedded context implementation
- [x] Phase 2: Update AI Module (4 tasks)
  - [x] 2.1 Remove dynamic context building
  - [x] 2.2 Add embedded context include
  - [x] 2.3 Update jshell_ai_execute_query()
  - [x] 2.4 Verify jshell_ai_chat() (no changes needed)
- [x] Phase 3: Update Build System (2 tasks)
  - [x] 3.1 Update Makefile
  - [x] 3.2 Verify compiler support
- [x] Phase 4: Simplify AIContext Structure (3 tasks)
  - [x] 4.1 Remove unused fields
  - [x] 4.2 Update initialization
  - [x] 4.3 Update cleanup
- [x] Phase 5: Testing (3 tasks)
  - [x] 5.1 Build and compile test
  - [x] 5.2 Functional testing
  - [x] 5.3 Context verification

**Total: 14 tasks - ALL COMPLETED**
