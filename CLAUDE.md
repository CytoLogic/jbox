# jbox Project

A custom shell implementation with CLI tools for agent MCP integration.

## Key Documentation

@ai/plans/AI_TODO_1.md - Current implementation plan and task tracking
@ai/CLItools.md - CLI tools specification for agent integration
@ai/APPANATOMY.md - Standard command anatomy and structure
@ai/CONVENTIONS.md - Code style and conventions

## Code Conventions

- Use GNU C23 standard for all C code
- 2 spaces for indentation
- 80 character line limit
- snake_case for functions and variables
- PascalCase for structs and types
- ALL_CAPS for global constants
- Use argtable3 for CLI argument parsing
- All agent-facing commands must support `--json` output
- All commands must support `-h` / `--help`

## Build Commands

- `make jbox DEBUG=1` - Build the shell with debug flags
- `make apps DEBUG=1` - Build standalone app binaries
- `make ls-app DEBUG=1` - Build standalone ls binary

## Test Commands

- `python -m unittest tests.apps.ls.test_ls -v` - Run ls tests

## Project Structure

- `src/jshell/builtins/` - Shell builtin command implementations
- `src/apps/<cmd>/` - Standalone app wrappers
- `tests/apps/<cmd>/` - Python unittest test files
