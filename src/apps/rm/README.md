# rm

Remove files or directories.

## Synopsis

```
rm [-hrf] [--json] FILE [FILE]...
```

## Description

Remove (unlink) the FILE(s). With `-r`, remove directories and their contents
recursively. With `-f`, ignore nonexistent files and never prompt.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-r, --recursive` | Remove directories and their contents recursively |
| `-f, --force` | Ignore nonexistent files, never prompt |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `FILE` | Files or directories to remove (one or more required) |

## Examples

Remove a single file:
```
rm file.txt
```

Remove multiple files:
```
rm file1.txt file2.txt file3.txt
```

Remove a directory recursively:
```
rm -r directory/
```

Force remove (ignore errors):
```
rm -f nonexistent.txt
```

Remove directory recursively without prompting:
```
rm -rf old_directory/
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "status": "success",
  "removed": ["file1.txt", "file2.txt"]
}
```

## Exit Status

- `0` - Success
- `1` - Error (file not found without -f, permission denied, etc.)
