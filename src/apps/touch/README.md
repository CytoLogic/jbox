# touch

Change file timestamps.

## Synopsis

```
touch [-h] [--json] FILE [FILE]...
```

## Description

Update the access and modification times of each FILE to the current time.
A FILE argument that does not exist is created empty.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `FILE` | Files to create or update (one or more required) |

## Examples

Create an empty file:
```
touch newfile.txt
```

Update timestamp on existing file:
```
touch existingfile.txt
```

Create/update multiple files:
```
touch file1.txt file2.txt file3.txt
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "status": "success",
  "touched": ["file1.txt", "file2.txt"]
}
```

## Exit Status

- `0` - Success
- `1` - Error (permission denied, etc.)
