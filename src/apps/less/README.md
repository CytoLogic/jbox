# less

View file contents with paging.

## Synopsis

```
less [-hN] [FILE]
```

## Description

View FILE contents with paging. Supports navigation with arrow keys, j/k,
space/b, and search with /pattern.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-N` | Show line numbers |

## Arguments

| Argument | Description |
|----------|-------------|
| `FILE` | File to view (optional, reads stdin if omitted) |

## Navigation Commands

| Key | Action |
|-----|--------|
| `j`, `DOWN` | Scroll down one line |
| `k`, `UP` | Scroll up one line |
| `SPACE`, `f` | Scroll down one page |
| `b` | Scroll up one page |
| `g` | Go to beginning |
| `G` | Go to end |
| `/pattern` | Search forward |
| `n` | Next search match |
| `N` | Previous search match |
| `q` | Quit |

## Examples

View a file:
```
less file.txt
```

View a file with line numbers:
```
less -N file.txt
```

## Exit Status

- `0` - Success
- `1` - Error (file not found, permission denied, etc.)
