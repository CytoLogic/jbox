# pkg - Package Manager

Manage jshell packages.

## Usage

```
pkg [OPTIONS] COMMAND [ARGS...]
```

## Options

- `-h, --help` - Display help and exit
- `--json` - Output in JSON format (where applicable)

## Commands

### list

List installed packages.

```
pkg list [--json]
```

### info NAME

Show information about a package.

```
pkg info NAME [--json]
```

### search NAME

Search for packages in registry.

```
pkg search NAME [--json]
```

### install NAME

Install a package.

```
pkg install NAME
```

### remove NAME

Remove an installed package.

```
pkg remove NAME
```

### build

Build a package from the current directory.

```
pkg build
```

### check-update

Check for available updates.

```
pkg check-update [--json]
```

### upgrade

Upgrade all packages to latest versions.

```
pkg upgrade
```

### compile

Compile package from source.

```
pkg compile
```

## JSON Output

When `--json` is specified, output is in JSON format:

```json
{
  "status": "ok|error|not_implemented",
  "message": "...",
  ...
}
```
