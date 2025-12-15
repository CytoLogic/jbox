# cat

Concatenate files and print on standard output.

## Synopsis

```
cat [-h] [--json] FILE [FILE]...
```

## Description

Concatenate FILE(s) to standard output. With `--json`, outputs each file as a
JSON object with path and content fields.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `FILE` | Files to concatenate (one or more required) |

## Examples

Print a single file:
```
cat file.txt
```

Concatenate multiple files:
```
cat file1.txt file2.txt file3.txt
```

Output in JSON format:
```
cat --json file.txt
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "path": "file.txt",
  "content": "file contents here..."
}
```

## Exit Status

- `0` - Success
- `1` - Error (file not found, permission denied, etc.)
