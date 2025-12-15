# cp

Copy files and directories.

## Synopsis

```
cp [-hrf] [--json] SOURCE DEST
```

## Description

Copy SOURCE to DEST, or copy SOURCE into DEST directory. With `-r`, copy
directories recursively. With `-f`, overwrite existing destination files.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-r, --recursive` | Copy directories recursively |
| `-f, --force` | Overwrite existing files |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `SOURCE` | Source file or directory |
| `DEST` | Destination path |

## Examples

Copy a file:
```
cp source.txt dest.txt
```

Copy a file into a directory:
```
cp file.txt /path/to/directory/
```

Copy a directory recursively:
```
cp -r src_dir dest_dir
```

Force overwrite existing file:
```
cp -f source.txt existing.txt
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "status": "success",
  "source": "source.txt",
  "destination": "dest.txt"
}
```

## Exit Status

- `0` - Success
- `1` - Error (file not found, permission denied, etc.)
