# ftp

FTP client for file transfer.

## Synopsis

```
ftp [-h] [-H <host>] [-p <port>] [-u <user>] [--json]
```

## Description

Connect to an FTP server for file upload and download. The client enters
interactive mode after connecting, providing commands for file operations.

## Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Display help and exit |
| `-H, --host <host>` | Server hostname (default: localhost) |
| `-p, --port <port>` | Server port (default: 21021) |
| `-u, --user <user>` | Username for login (default: anonymous) |
| `--json` | Output in JSON format |

## Interactive Commands

| Command | Description |
|---------|-------------|
| `ls [path]` | List directory contents |
| `cd <path>` | Change directory |
| `pwd` | Print working directory |
| `get <remote> [local]` | Download file |
| `put <local> [remote]` | Upload file |
| `mkdir <dir>` | Create directory |
| `help` | Show available commands |
| `quit` | Disconnect and exit |

## Examples

Connect to local FTP server:
```
ftp
```

Connect to specific host and port:
```
ftp -H ftp.example.com -p 21
```

Connect with username:
```
ftp -H localhost -p 21021 -u myuser
```

Interactive session example:
```
$ ftp -H localhost -p 21021
Connecting to localhost:21021...
Connected: 220 Welcome to jbox FTP server.
Logging in as anonymous...
Logged in: 230 User anonymous logged in.
Type 'help' for available commands.
ftp> ls
drwxr-xr-x  2 user user  4096 Dec 16 12:00 uploads
-rw-r--r--  1 user user   123 Dec 16 12:00 README.txt
ftp> cd uploads
Changed to uploads
ftp> put myfile.txt
Uploaded myfile.txt -> myfile.txt
ftp> get remotefile.txt localcopy.txt
Downloaded remotefile.txt -> localcopy.txt
ftp> quit
Goodbye.
```

## JSON Output

When `--json` is specified, all output is formatted as JSON objects:

Connection:
```json
{"action":"connect","host":"localhost","port":21021,"status":"ok","response":"220 Welcome"}
{"action":"login","user":"anonymous","status":"ok","response":"230 User logged in"}
```

List directory:
```json
{"action":"ls","status":"ok","listing":"drwxr-xr-x  2 user user  4096 ..."}
```

File transfer:
```json
{"action":"get","status":"ok","remote":"file.txt","local":"file.txt"}
{"action":"put","status":"ok","local":"file.txt","remote":"file.txt"}
```

Change directory:
```json
{"action":"cd","status":"ok","path":"/uploads"}
```

Print working directory:
```json
{"action":"pwd","status":"ok","path":"/"}
```

Create directory:
```json
{"action":"mkdir","status":"ok","dir":"newdir"}
```

Error:
```json
{"action":"get","status":"error","remote":"missing.txt","message":"550 File not found"}
```

## Exit Status

- `0` - Success
- `1` - Error (connection failed, invalid arguments, etc.)
