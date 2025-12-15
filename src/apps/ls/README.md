# ls

List directory contents.

## Synopsis

```
ls [-hal] [--json] [PATH]...
```

## Description

List information about the FILEs (the current directory by default).
Entries are sorted alphabetically.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-a` | Do not ignore entries starting with `.` |
| `-l` | Use long listing format |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `PATH` | Files or directories to list (optional, defaults to `.`) |

## Examples

List current directory:
```
ls
```

List with hidden files:
```
ls -a
```

Long format listing:
```
ls -l
```

List specific directory:
```
ls /path/to/directory
```

List multiple paths:
```
ls dir1 dir2 file.txt
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
[
  {
    "name": "file.txt",
    "type": "file",
    "size": 1234,
    "mode": "-rw-r--r--",
    "mtime": "2024-12-14T10:30:00Z"
  }
]
```

## Exit Status

- `0` - Success
- `1` - Error (directory not found, permission denied, etc.)
