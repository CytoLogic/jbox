# mkdir

Make directories.

## Synopsis

```
mkdir [-hp] [--json] DIR [DIR]...
```

## Description

Create the DIRECTORY(ies), if they do not already exist. With `-p`, create
parent directories as needed.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-p, --parents` | Make parent directories as needed |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `DIR` | Directories to create (one or more required) |

## Examples

Create a single directory:
```
mkdir newdir
```

Create multiple directories:
```
mkdir dir1 dir2 dir3
```

Create nested directories:
```
mkdir -p path/to/nested/directory
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "status": "success",
  "created": ["dir1", "dir2"]
}
```

## Exit Status

- `0` - Success
- `1` - Error (permission denied, directory exists, etc.)
