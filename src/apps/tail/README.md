# tail

Output the last part of files.

## Synopsis

```
tail [-h] [-n N] [--json] FILE
```

## Description

Print the last N lines of FILE to standard output. With `--json`, outputs a
JSON object with path and lines array.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-n N` | Output the last N lines (default 10) |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `FILE` | File to read |

## Examples

Print last 10 lines (default):
```
tail file.txt
```

Print last 20 lines:
```
tail -n 20 file.txt
```

Output in JSON format:
```
tail --json file.txt
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "path": "file.txt",
  "lines": [
    "line n-9",
    "line n-8",
    "...",
    "line n"
  ]
}
```

## Exit Status

- `0` - Success
- `1` - Error (file not found, permission denied, etc.)
