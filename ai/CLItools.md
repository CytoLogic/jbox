CLI Tools Specification for Agent MCP Integration
This document defines the minimum set of CLI tools and options that the course shell must implement to serve as a backend for agents (MCP-style) and for human users.


Design rules:


* The shell performs wildcard expansion (globs) on arguments: *, ?, [...].
* Commands that support regular expressions are explicitly noted; commands must not re-interpret shell globs as regex.
* Every command supports:
   * -h – human-readable help.
   * --help – synonym for -h (optional but recommended).
* Agent-oriented commands should support:
   * --json – machine-readable output (JSON) with stable structure.


________________


1. Agent-Facing Core Tools
These commands form the core capability set for MCP-style agent integration. They should be implemented as built-ins where feasible, using internal APIs and threads rather than spawning external processes.
1.1 Filesystem
ls – list directory contents
Options:


* -h, --help – show usage and human description.
* -a – include entries starting with ..
* -l – long format (permissions, owner, size, timestamps).
* --json – output an array of entries with fields like name, type, size, mtime.


stat – file metadata
Options:


* -h, --help
* --json – output object with fields such as path, type, size, mode, mtime, atime, ctime.


cat – print file contents
Options:


* -h, --help
* Optional: --json – wrap output as { "path": "...", "content": "..." }.


head / tail – view start/end of file
Options:


* -h, --help
* -n N – number of lines (default e.g. 10).
* --json – { "path": "...", "lines": ["...", "..."] }.


cp – copy files/directories
Options:


* -h, --help
* -r – recursive copy.
* -f – force overwrite.
* --json – structured success/error summary.


mv – move/rename files/directories
Options:


* -h, --help
* -f – force overwrite.
* --json.


rm – remove files/directories
Options:


* -h, --help
* -r – recursive.
* -f – force (no prompt).
* --json.


mkdir – create directories
Options:


* -h, --help
* -p – create parent directories as needed.
* --json.


rmdir – remove empty directories
Options:


* -h, --help
* --json.


touch – create empty file or update timestamps
Options:


* -h, --help
* --json.
1.2 Search and Text (regex-capable)
rg – search text with regex (preferred over old grep)
Regex-aware: YES
Options:


* -h, --help
* -n – show line numbers.
* -i – case-insensitive.
* -w – match whole words.
* -C N – show N lines of context.
* --fixed-strings – treat pattern as literal (no regex).
* --json – emit JSON objects per match (file, line, column, text).


Shell globs (*.c) are expanded before rg sees the filenames; rg interprets its pattern as regex unless --fixed-strings is used.
1.3 Structured Editing Commands
These are simple, deterministic editing tools designed for agents; they operate on complete files, not streams.


edit-replace-line – replace a single line in a file
Usage:


* edit-replace-line FILE N TEXT
Options:
* -h, --help
* --json – { "path": "...", "line": N, "status": "ok" | "error", "message": "..." }.


edit-insert-line – insert a line before a given line number
Usage:


   * edit-insert-line FILE N TEXT
Options:
   * -h, --help
   * --json.


edit-delete-line – delete a single line
Usage:


      * edit-delete-line FILE N
Options:
      * -h, --help
      * --json.


edit-replace – global find/replace with regex
Regex-aware: YES
Usage:


         * edit-replace FILE PATTERN REPLACEMENT
Options:
         * -h, --help
         * -i – case-insensitive regex.
         * --fixed-strings – treat PATTERN as literal.
         * --json – e.g. { "path": "...", "matches": M, "replacements": R }.
1.4 Processes and Jobs
jobs – list background/managed jobs (threads)
Options:


            * -h, --help
            * --json – list of job records: id, state, command, etc.


ps – list processes/threads known to the shell
Options:


            * -h, --help
            * --json – structured process list.


kill – send signal to a job or process
Options:


            * -h, --help
            * -s SIGNAL – specify signal (e.g. TERM, KILL).
            * --json.


wait – wait for a job to finish
Options:


            * -h, --help
            * --json – { "job": ID, "status": "exited", "code": N }.
1.5 Shell and Environment
cd – change directory
Options:


            * -h, --help.


pwd – print working directory
Options:


            * -h, --help
            * --json – { "cwd": "/path" }.


env – list environment variables
Options:


            * -h, --help
            * --json – { "env": { "KEY": "VALUE", ... } }.


export – set environment variable
Usage:


            * export KEY=VALUE
Options:
            * -h, --help
            * --json.


unset – unset environment variable
Usage:


               * unset KEY
Options:
               * -h, --help
               * --json.


type – show how a name is resolved
Options:


                  * -h, --help
                  * --json – { "name": "ls", "kind": "builtin" | "external" | "alias", "path": "/bin/ls" }.
1.6 Networking (later)
http-get – fetch a URL
Options:


                  * -h, --help
                  * -H KEY:VALUE – add header (repeatable).
                  * --json – structured response: status, headers, body (maybe truncated or base64).


http-post – POST data to a URL
Options:


                  * -h, --help
                  * -H KEY:VALUE – header.
                  * -d DATA – inline body data.
                  * --json.
1.7 Package / system info (optional but useful)
pkg – package manager (from the PackageManagement chapter)
Suggested MCP-friendly subcommands:


                  * pkg list --json – installed packages.
                  * pkg info NAME --json – details for one package.
                  * pkg search NAME --json – from registry.


________________


2. Human-Facing CLI Tools
These commands are primarily for interactive use. They also must support -h / --help for human-readable usage, but --json is optional unless you want agents to use them directly.
2.1 Editing and viewing
vi (or nvi/vim-style minimal editor)
Options:


                  * -h, --help
                  * Standard vi-like command set; no --json.


less – pager for viewing text
Options:


                  * -h, --help
                  * -N – show line numbers.
                  * No --json (pager is human-only).
2.2 Help and documentation
help – shell built-in help
Usage:


                  * help – list built-in commands.
                  * help CMD – show brief usage for a command.
Options:
                  * -h, --help.


man – manual pages (optional if you rely on --help)
Options:


                     * -h, --help.
2.3 Shell usability
history – show command history
Options:


                     * -h, --help.


alias / unalias – manage aliases
Options:


                     * -h, --help.


echo – print text
Options:


                     * -h, --help.


printf – formatted output
Options:


                     * -h, --help.
2.4 Miscellaneous
sleep – delay
Options:


                     * -h, --help.


date – show system time
Options:


                     * -h, --help.


true / false – do nothing, succeed/fail
Options:


                     * -h, --help.


________________


3. Regex vs Shell Wildcards – Design Clarification
                     * The shell is responsible for expanding wildcards (globs) in arguments:
                     * *.c, src/*.h, file?.txt, [a-z]*.log, etc.
                     * Commands receive expanded paths, not raw patterns.
                     * Commands use regex only where explicitly designed to:
                     * rg – for pattern matching in file contents.
                     * edit-replace – for search/replace within a file.
                     * Commands must not interpret shell globs as regex; they should either:
                     * Treat arguments as literal strings (paths, plain text), or
                     * Treat specific pattern arguments as regex (documented clearly).


This separation keeps the mental model clean and matches Unix conventions:


                     * Use shell wildcards for selecting files.
                     * Use regex within commands for matching text.
