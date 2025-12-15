# head

Output the first part of files.

## Synopsis

```
head [-h] [-n N] [--json] FILE
```

## Description

Print the first N lines of FILE to standard output. With `--json`, outputs a
JSON object with path and lines array.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-n N` | Output the first N lines (default 10) |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `FILE` | File to read |

## Examples

Print first 10 lines (default):
```
head file.txt
```

Print first 5 lines:
```
head -n 5 file.txt
```

Output in JSON format:
```
head --json file.txt
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "path": "file.txt",
  "lines": [
    "line 1",
    "line 2",
    "..."
  ]
}
```

## Exit Status

- `0` - Success
- `1` - Error (file not found, permission denied, etc.)
