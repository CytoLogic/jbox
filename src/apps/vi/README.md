# vi

Edit files with vi-like interface.

## Synopsis

```
vi [-h] [FILE]
```

## Description

Edit FILE with a vi-like text editor. Supports normal, insert, and command
modes. Basic navigation with hjkl, editing with i/a/o/x/dd/yy/p, and file
operations with : commands.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |

## Arguments

| Argument | Description |
|----------|-------------|
| `FILE` | File to edit (optional, starts with empty buffer if omitted) |

## Normal Mode Commands

### Navigation

| Key | Action |
|-----|--------|
| `h`, `LEFT` | Move cursor left |
| `j`, `DOWN` | Move cursor down |
| `k`, `UP` | Move cursor up |
| `l`, `RIGHT` | Move cursor right |
| `0` | Move to beginning of line |
| `$` | Move to end of line |
| `gg` | Go to first line |
| `G` | Go to last line |
| `w` | Move forward by word |
| `b` | Move backward by word |

### Entering Insert Mode

| Key | Action |
|-----|--------|
| `i` | Enter insert mode at cursor |
| `a` | Enter insert mode after cursor |
| `o` | Open line below and enter insert mode |
| `O` | Open line above and enter insert mode |

### Editing

| Key | Action |
|-----|--------|
| `x` | Delete character under cursor |
| `dd` | Delete current line |
| `yy` | Yank (copy) current line |
| `p` | Paste after cursor/line |
| `P` | Paste before cursor/line |
| `u` | Undo (limited) |

### Other

| Key | Action |
|-----|--------|
| `:` | Enter command mode |
| `/` | Search forward |

## Command Mode

| Command | Action |
|---------|--------|
| `:w` | Write (save) file |
| `:q` | Quit (fails if modified) |
| `:q!` | Quit without saving |
| `:wq` | Write and quit |
| `:e FILE` | Edit FILE |

## Insert Mode

In insert mode, type normally to insert text. Press `ESC` to return to
normal mode.

## Examples

Edit an existing file:
```
vi file.txt
```

Create a new file:
```
vi newfile.txt
```

## Exit Status

- `0` - Success
- `1` - Error (file not found for reading, permission denied, etc.)
