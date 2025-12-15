# rg

Search for patterns using regular expressions.

## Synopsis

```
rg [-hniw] [-C N] [--fixed-strings] [--json] PATTERN [FILE]...
```

## Description

Search for PATTERN in each FILE or standard input. PATTERN is a POSIX extended
regular expression by default. Use `--fixed-strings` to treat PATTERN as a
literal string.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-n` | Show line numbers |
| `-i` | Case-insensitive search |
| `-w` | Match whole words only |
| `-C N` | Show N lines of context |
| `--fixed-strings` | Treat pattern as literal string |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `PATTERN` | Search pattern (regex by default) |
| `FILE` | Files to search (optional, reads stdin if omitted) |

## Examples

Search for a pattern in a file:
```
rg "error" logfile.txt
```

Case-insensitive search:
```
rg -i "warning" *.log
```

Search with line numbers:
```
rg -n "TODO" src/*.c
```

Match whole words only:
```
rg -w "main" program.c
```

Show context lines:
```
rg -C 2 "function" script.js
```

Literal string search (no regex):
```
rg --fixed-strings "foo.bar()" code.py
```

## JSON Output

When `--json` is specified, each match is output as:
```json
{
  "file": "src/main.c",
  "line": 42,
  "column": 10,
  "text": "    return main();"
}
```

## Exit Status

- `0` - Matches found
- `1` - No matches found or error
