# rmdir

Remove empty directories.

## Synopsis

```
rmdir [-h] [--json] DIR [DIR]...
```

## Description

Remove the DIRECTORY(ies), if they are empty.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `DIR` | Directories to remove (one or more required) |

## Examples

Remove an empty directory:
```
rmdir empty_dir
```

Remove multiple empty directories:
```
rmdir dir1 dir2 dir3
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "status": "success",
  "removed": ["dir1", "dir2"]
}
```

## Exit Status

- `0` - Success
- `1` - Error (directory not empty, not found, permission denied, etc.)

## Notes

To remove non-empty directories, use `rm -r` instead.
