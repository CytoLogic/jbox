# sleep

Delay for a specified amount of time.

## Synopsis

```
sleep [-h] SECONDS
```

## Description

Pause execution for SECONDS. The argument may be a floating point number to
specify fractional seconds.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |

## Arguments

| Argument | Description |
|----------|-------------|
| `SECONDS` | Pause duration (can be fractional) |

## Examples

Sleep for 5 seconds:
```
sleep 5
```

Sleep for half a second:
```
sleep 0.5
```

Sleep for 2.5 seconds:
```
sleep 2.5
```

## Exit Status

- `0` - Success
- `1` - Error (invalid argument)
