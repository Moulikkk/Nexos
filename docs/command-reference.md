# Command reference

## Start the shell

From the project root:

```sh
./build/nex
```

The interactive prompt is `nex$`.

## Built-in commands

Built-ins are handled by the shell process itself.

| Command | Usage | Description |
|---|---|---|
| `help` | `help` | Displays shell help |
| `cd` | `cd [directory]` | Changes the shell's working directory |
| `exit` | `exit` | Exits the shell |

### `help`

```sh
nex$ help
```

Displays the help text implemented by the running version.

### `cd`

```sh
nex$ cd /tmp
nex$ cd ..
```

`cd` must run in the shell process itself; running it only in a child would not change the parent shell's working directory. Check the implementation/help output for the exact behavior when no directory is supplied.

### `exit`

```sh
nex$ exit
```

Terminates the interactive shell.

## External commands

Commands that are not built-ins are launched as external programs. The process implementation uses `execvp()`, which searches `PATH`.

```sh
nex$ pwd
nex$ ls -la
nex$ echo Hello Nexos
nex$ ps aux
```

These are host-system programs, not Nexos built-ins. Their availability depends on the operating system and `PATH`.

## Arguments and quoting

Arguments are passed to the executable as separate arguments. The shell supports simple quoting, for example:

```sh
nex$ echo "hello from Nexos"
```

This is not a complete Bash-compatible parser. Do not assume support for variable expansion (`$HOME`), command substitution (`$(...)`), wildcard expansion (`*.cpp`), aliases, functions, or background jobs.

## Pipelines

Use `|` to connect one command's output to the next command's input:

```sh
nex$ ls -l | grep cpp
nex$ ls -l | grep cpp | wc -l
nex$ ps aux | grep bash
```

For `ls -l | grep cpp | wc -l`:

1. `ls -l` produces a listing.
2. The first pipe sends it to `grep cpp`.
3. `grep cpp` keeps matching lines.
4. The second pipe sends matches to `wc -l`.
5. `wc -l` counts the lines.

Each stage must contain a command. An empty stage such as `ls | | wc` is rejected.

Shell redirection operators such as `>`, `>>`, and `<` are not documented as supported syntax.

## Process monitor

`nexps` is a separate executable, not a shell built-in.

One-time snapshot:

```sh
./build/nexps --once
```

Live monitor:

```sh
./build/nexps
```

The live display refreshes approximately every second. Press `Ctrl+C` to stop it.

Displayed fields include PID, PPID, state, resident memory, CPU ticks, and command name. This application depends on Linux `/proc`.

## Build and test commands

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

- Configure: generate build files.
- Build: compile the library, applications, and tests.
- Test: execute registered CTest tests.
