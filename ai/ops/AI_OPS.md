# AI-Assisted Development Strategy

This document outlines the prompting strategy and workflow used to develop the jbox project with AI assistance. The approach emphasizes structured planning, incremental implementation, and maintainable context management.

---

## Overview

The development process follows a **plan-implement-verify-commit** cycle that maximizes AI effectiveness while maintaining code quality and project coherence across sessions.

```
┌─────────────────────────────────────────────────────────────────┐
│                      Development Cycle                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐ │
│   │  Plan    │───>│ Implement│───>│  Test    │───>│  Commit  │ │
│   └──────────┘    └──────────┘    └──────────┘    └──────────┘ │
│        │                                               │        │
│        │              Context Reset                    │        │
│        └───────────────────────────────────────────────┘        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Branching Strategy

AI-assisted development uses a dedicated branch to isolate AI work from the main development flow:

```
master
  │
  └──> dev/feature-branch
          │
          └──> ai (AI working branch)
                │
                ├── commit: implement phase 1
                ├── commit: implement phase 2
                └── commit: implement phase 3
                │
          <─────┘ (merge back to dev/feature)
          │
  <───────┘ (merge to master)
```

### Workflow

1. **Create AI branch** - Branch off from the current development or feature branch
2. **AI performs work** - All AI-assisted implementation happens on the `ai` branch
3. **Merge to dev/feature** - Once complete, merge the `ai` branch back into the development or feature branch
4. **Merge to master** - The dev/feature branch is then merged into `master`

### Benefits

- **Isolation** - AI work is isolated from ongoing development
- **Review opportunity** - Changes can be reviewed before merging to dev
- **Easy rollback** - If AI-generated code has issues, the branch can be discarded
- **Clean history** - Squash merges can consolidate AI commits if desired

---

## Project Setup

### 1. Establish Conventions Early

Before beginning implementation, establish clear conventions that the AI will follow:

- **`CONVENTIONS.md`** - Code style, naming conventions, standards (C23, indentation, etc.)
- **`APPANATOMY.md`** - Standard command structure and module organization
- **`CLItools.md`** - CLI specification for agent-facing commands
- **`CLAUDE.md`** - Project-level instructions automatically read by the AI

These specification files (particularly `APPANATOMY.md` and `CLItools.md`) are written or generated early in the project to document project-wide architectural decisions. They define the target architecture and conventions that features should follow, even before those features are implemented. This allows the AI to consistently implement new features against a predetermined design, ensuring coherence across the codebase.

The AI reads these files to ensure consistent code generation across all sessions.

### 2. Standardize Testing

Choose a single testing framework and format:

- **Python unittest** for all tests in this project
- Consistent test file naming: `test_<component>.py`
- Consistent test directory structure: `tests/<category>/<component>/`
- Tests are generated alongside implementation

---

## Feature Implementation Workflow

### Phase 1: Planning

#### Write a Detailed Specification

Create a comprehensive prompt outlining:

- Feature requirements and behavior
- Input/output specifications
- Edge cases and error handling
- Integration points with existing code

**Example prompt structure:**
```
Implement <feature> for the jshell project.

Requirements:
- <requirement 1>
- <requirement 2>

Behavior:
- <expected behavior>

Integration:
- <how it connects to existing code>

Output an implementation plan as AI_TODO_<N>.md
```

#### Generate Implementation Plan

Ask the AI to output an `AI_TODO_<N>.md` file containing:

- Phase breakdown with clear milestones
- Task checklists with `[ ]` checkboxes
- File locations for new/modified code
- Dependencies between tasks
- Testing requirements

#### Verify the Plan

Review the generated plan before proceeding:

- Check for completeness
- Verify architectural decisions
- Ensure alignment with project conventions
- Adjust scope if necessary

### Phase 2: Implementation

#### Instruct the AI to Execute

Provide clear implementation instructions:

```
Proceed with the implementation plan in AI_TODO_<N>.md.

Constraints:
- Check off todos as they are completed
- Generate tests in the appropriate directory
- Update Makefile targets when necessary
- Follow conventions in CONVENTIONS.md
- Generate Doxygen-style docstrings
- Commit changes after each implementation phase
```

#### Incremental Progress

The AI works through the plan:

1. Implements code for current phase
2. Generates corresponding tests
3. Updates build system (Makefile)
4. Marks tasks complete in AI_TODO_<N>.md
5. Commits with descriptive message

### Phase 3: Context Management

#### Reset Context Between Phases

After each major phase or commit:

1. **Reset the conversation context** (start fresh session)
2. Instruct the AI to continue from where it left off:

```
Continue with the implementation plan in AI_TODO_<N>.md.

Start from Phase <X> where the previous session ended.

Same constraints apply:
- Check off todos as completed
- Generate tests as necessary
- Update Makefile when needed
- Commit after each phase
```

This approach:
- Prevents context overflow in long implementations
- Maintains fresh, focused sessions
- Leverages the AI_TODO file as persistent state

---

## Quick Changes Workflow

For small changes, revisions, or fixes that don't require extensive planning:

```
┌─────────────────────────────────────────┐
│         Quick Change Workflow           │
├─────────────────────────────────────────┤
│                                         │
│   Change ──> Test ──> Commit            │
│                                         │
│   (Single session, no plan required)    │
│                                         │
└─────────────────────────────────────────┘
```

- No implementation plan generated
- Changes are made directly
- Tests are run to verify
- Committed in a single session

**Examples:**
- Bug fixes
- Documentation updates
- Small refactors
- Configuration changes

---

## Debugging Workflow

When tests fail or bugs are detected:

### 1. Reset Context (if necessary)

Start with a fresh context to avoid confusion from previous attempts.

### 2. Provide Clear Bug Report

```
A bug has been detected in <component>.

Symptoms:
- <what is happening>

Expected behavior:
- <what should happen>

Relevant files:
- <file paths>

Test output:
<paste failing test output>

Debug and fix this issue.
```

### 3. Iterative Debugging

The AI will:
1. Analyze the bug
2. Identify root cause
3. Propose and implement fix
4. Run tests to verify
5. Commit the fix

---

## Documentation Standards

### Doxygen-Style Docstrings

All functions include documentation:

```c
/**
 * @brief Short description of the function
 *
 * Longer description if needed, explaining behavior,
 * edge cases, and important details.
 *
 * @param param1 Description of first parameter
 * @param param2 Description of second parameter
 * @return Description of return value
 */
int function_name(int param1, char *param2);
```

### AI_TODO File Format

Implementation plans follow a consistent structure:

```markdown
# Feature Implementation Plan

## Overview
Brief description of what's being implemented.

## Phase 1: Component Name
### 1.1 Task Group
- [ ] Specific task
- [ ] Another task

### 1.2 Another Task Group
- [ ] Task with details

## Phase 2: Next Component
...

## Notes
Additional context, dependencies, considerations.
```

---

## Best Practices

### Do

- **Be specific** in requirements and constraints
- **Break large features** into multiple phases
- **Reset context** between major phases
- **Verify plans** before implementation
- **Test incrementally** as features are built
- **Commit frequently** with descriptive messages

### Don't

- Don't let context grow too large
- Don't skip the planning phase for complex features
- Don't implement without tests
- Don't ignore failing tests
- Don't forget to update build system

---

## Session Templates

### New Feature Session

```
Implement <feature> for the jbox project.

<detailed requirements>

Output an implementation plan as AI_TODO_<N>.md following
the format in existing AI_TODO files.
```

### Continue Implementation Session

```
Continue with the implementation plan in ai/plans/AI_TODO_<N>.md.

Resume from Phase <X>.

Constraints:
- Check off todos as completed
- Generate tests in tests/<category>/
- Update Makefile when needed
- Use Doxygen-style docstrings
- Follow CONVENTIONS.md
- Commit after each phase
```

### Bug Fix Session

```
Debug the following issue in <component>:

<bug description and test output>

Fix the bug and verify with tests.
```

### Quick Change Session

```
<describe the change needed>

Make the change, test it, and commit.
```

---

## Summary

| Scenario | Approach |
|----------|----------|
| New major feature | Plan → Implement → Test → Commit → Reset → Repeat |
| Small change | Change → Test → Commit (single session) |
| Bug fix | Reset → Debug → Fix → Test → Commit |
| Documentation | Write → Review → Commit |

This strategy enables efficient AI-assisted development while maintaining code quality, consistency, and project coherence across extended development efforts.
