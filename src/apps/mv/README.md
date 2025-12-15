# mv

Move (rename) files.

## Synopsis

```
mv [-hf] [--json] SOURCE DEST
```

## Description

Rename SOURCE to DEST, or move SOURCE into DEST directory. With `-f`,
overwrite existing destination files.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-f, --force` | Overwrite existing files |
| `--json` | Output in JSON format |

## Arguments

| Argument | Description |
|----------|-------------|
| `SOURCE` | Source file or directory |
| `DEST` | Destination path |

## Examples

Rename a file:
```
mv oldname.txt newname.txt
```

Move a file into a directory:
```
mv file.txt /path/to/directory/
```

Force overwrite existing file:
```
mv -f source.txt existing.txt
```

## JSON Output

When `--json` is specified, output is formatted as:
```json
{
  "status": "success",
  "source": "oldname.txt",
  "destination": "newname.txt"
}
```

## Exit Status

- `0` - Success
- `1` - Error (file not found, permission denied, etc.)
