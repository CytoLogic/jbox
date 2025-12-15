# stat

Display file metadata.

## Synopsis

```
stat [-h] [--json] FILE
```

## Description

Display detailed information about a file including type, size, permissions,
ownership, and timestamps.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `FILE` | File to get metadata for |

## Examples

Display file information:
```
stat file.txt
```

Output in JSON format:
```
stat --json file.txt
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "path": "file.txt",
  "type": "regular file",
  "size": 1234,
  "mode": "-rw-r--r--",
  "uid": 1000,
  "gid": 1000,
  "atime": "2024-12-14T10:30:00Z",
  "mtime": "2024-12-14T09:15:00Z",
  "ctime": "2024-12-14T09:15:00Z"
}
```

## Exit Status

- `0` - Success
- `1` - Error (file not found, permission denied, etc.)
