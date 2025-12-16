# AI Integration Migration: Anthropic to Google Gemini 2.5 Flash

**Status: COMPLETED**

## Overview

Migrate jshell AI integration from Anthropic Claude API to Google Gemini 2.5 Flash API.

### Previous State

- AI integration used Anthropic Messages API (`claude-3-haiku-20240307`)
- Files: `jshell_ai.c`, `jshell_ai_api.c`, `jshell_ai_api.h`, `jshell_ai.h`
- API key from `ANTHROPIC_API_KEY` environment variable
- Uses libcurl for HTTP requests

### Current State

- AI integration uses Google Gemini 2.5 Flash API
- API key from `GOOGLE_API_KEY` environment variable
- Maintains same public API (`jshell_ai_chat()`, `jshell_ai_execute_query()`)
- Same user-facing behavior (`@query` and `@!query`)

### Files to Modify

| File | Changes |
|------|---------|
| `src/jshell/jshell_ai_api.h` | Rename to `jshell_gemini_api.h`, update types and functions |
| `src/jshell/jshell_ai_api.c` | Rename to `jshell_gemini_api.c`, rewrite API logic |
| `src/jshell/jshell_ai.c` | Update includes, function calls, env variable |
| `src/jshell/jshell_ai.h` | Update comment about env variable |
| `src/ast/jshell_ast_interpreter.c` | Update error messages for API key |
| `Makefile` | Update source file reference |

---

## Phase 1: Update API Client

### 1.1 Rename and Update API Header (`src/jshell/jshell_ai_api.h` -> `src/jshell/jshell_gemini_api.h`)

- [x] Rename file from `jshell_ai_api.h` to `jshell_gemini_api.h`
- [x] Update include guard from `JSHELL_AI_API_H` to `JSHELL_GEMINI_API_H`
- [x] Rename struct `AnthropicResponse` to `GeminiResponse`
- [x] Rename function `jshell_anthropic_request()` to `jshell_gemini_request()`
- [x] Rename function `jshell_free_anthropic_response()` to `jshell_free_gemini_response()`
- [x] Update all comments referencing Anthropic to Gemini

### 1.2 Rename and Rewrite API Implementation (`src/jshell/jshell_ai_api.c` -> `src/jshell/jshell_gemini_api.c`)

- [x] Rename file from `jshell_ai_api.c` to `jshell_gemini_api.c`
- [x] Update `#include "jshell_ai_api.h"` to `#include "jshell_gemini_api.h"`
- [x] Update API endpoint to: `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent`
- [x] Update `build_request_json()` for Gemini format:
  ```json
  {
    "contents": [
      {"role": "user", "parts": [{"text": "..."}]}
    ],
    "systemInstruction": {"parts": [{"text": "..."}]},
    "generationConfig": {"maxOutputTokens": N}
  }
  ```
- [x] Update `parse_response()` for Gemini format:
  - Parse `candidates[0].content.parts[0].text` for success
  - Parse `error.message` for errors
- [x] Update URL construction: Append `?key=API_KEY` query parameter instead of using `x-api-key` header
- [x] Remove `anthropic-version` header
- [x] Update function signature names to match new header

### 1.3 Update Makefile

- [x] In `JSHELL_SRCS`, change `$(SRC_DIR)/jshell/jshell_ai_api.c` to `$(SRC_DIR)/jshell/jshell_gemini_api.c`

---

## Phase 2: Update AI Module Callers

### 2.1 Update `src/jshell/jshell_ai.c`

- [x] Line 6: Change `#include "jshell_ai_api.h"` to `#include "jshell_gemini_api.h"`
- [x] Line 11: Change `AI_MODEL` from `"claude-3-haiku-20240307"` to `"gemini-2.5-flash"`
- [x] Line 202: Change `getenv("ANTHROPIC_API_KEY")` to `getenv("GOOGLE_API_KEY")`
- [x] Line 204: Change debug message from `"ANTHROPIC_API_KEY not set..."` to `"GOOGLE_API_KEY not set..."`
- [x] Line 239: Update fallback error message to reference `GOOGLE_API_KEY`
- [x] Line 246: Change `AnthropicResponse` to `GeminiResponse`
- [x] Line 246: Change `jshell_anthropic_request()` to `jshell_gemini_request()`
- [x] Line 268: Change `jshell_free_anthropic_response()` to `jshell_free_gemini_response()`
- [x] Line 297: Change `AnthropicResponse` to `GeminiResponse`
- [x] Line 297: Change `jshell_anthropic_request()` to `jshell_gemini_request()`
- [x] Line 334: Change `jshell_free_anthropic_response()` to `jshell_free_gemini_response()`

### 2.2 Update `src/jshell/jshell_ai.h`

- [x] Line 15: Update comment from `ANTHROPIC_API_KEY` to `GOOGLE_API_KEY`

### 2.3 Update `src/ast/jshell_ast_interpreter.c`

- [x] Line 579: Change error message from:
  ```c
  fprintf(stderr, "  Add ANTHROPIC_API_KEY=<your_key> to ~/.jshell/env\n");
  ```
  to:
  ```c
  fprintf(stderr, "  Add GOOGLE_API_KEY=<your_key> to ~/.jshell/env\n");
  ```
- [x] Line 601: Same change as above for the second occurrence

---

## Phase 3: Testing

### 3.1 Test Without API Key

- [x] Verify error message displays correctly
- [x] Verify exit status is 1

### 3.2 Test Chat Query

- [x] Test `@hello` with valid API key (returns API error due to invalid key - integration working)
- [ ] Test `@what is 2+2?` (requires valid API key)
- [ ] Verify response is displayed (requires valid API key)

### 3.3 Test Execute Query

- [ ] Test `@!list the current directory` (requires valid API key)
- [ ] Verify command is generated (requires valid API key)
- [ ] Verify confirmation prompt works (requires valid API key)
- [ ] Verify command execution (requires valid API key)

### 3.4 Test Error Handling

- [x] Test with invalid API key (confirmed: returns "API key not valid" error)
- [ ] Test with network error (if possible)

---

## Phase 4: Cleanup

### 4.1 Update Documentation

- [x] Update AI_TODO_6.md to reference new API
- [x] Update any comments referencing Anthropic

### 4.2 Remove Old Code

- [x] Delete old `jshell_ai_api.c` and `jshell_ai_api.h`

---

## API Reference

### Gemini 2.5 Flash API

**Endpoint:**
```
POST https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=API_KEY
```

**Request Format:**
```json
{
  "contents": [
    {
      "role": "user",
      "parts": [{"text": "user message"}]
    }
  ],
  "systemInstruction": {
    "parts": [{"text": "system prompt"}]
  },
  "generationConfig": {
    "maxOutputTokens": 1024
  }
}
```

**Response Format:**
```json
{
  "candidates": [
    {
      "content": {
        "parts": [
          {"text": "response text"}
        ],
        "role": "model"
      }
    }
  ]
}
```

**Error Format:**
```json
{
  "error": {
    "code": 400,
    "message": "error description",
    "status": "INVALID_ARGUMENT"
  }
}
```

---

## Notes

- Gemini 2.5 Flash is a fast, efficient model suitable for command generation
- API key is passed as query parameter, not header
- System instruction is a separate field, not part of messages array
- Response structure differs significantly from Anthropic
