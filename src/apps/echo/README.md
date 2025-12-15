# echo

Display a line of text.

## Synopsis

```
echo [-hn] [TEXT]...
```

## Description

Display the TEXT arguments separated by spaces, followed by a newline.
Use `-n` to suppress the trailing newline.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-n` | Do not output trailing newline |

## Arguments

| Argument | Description |
|----------|-------------|
| `TEXT` | Text to print (optional, multiple allowed) |

## Examples

Print a message:
```
echo Hello, world!
```

Print without trailing newline:
```
echo -n "Enter name: "
```

Print multiple arguments:
```
echo one two three
```
Output: `one two three`

## Exit Status

- `0` - Success
