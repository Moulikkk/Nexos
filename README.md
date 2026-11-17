# Nexos — A Unix Systems Abstraction Framework

Nexos is a C++20 library and set of demonstration applications that wrap selected Unix/Linux operating-system primitives in small RAII-based C++ abstractions. It demonstrates file descriptors, process creation and synchronization, unnamed pipes, and command pipelines.

## Components

- **`nexos::File`** — owns a file descriptor and wraps file opening, reading, writing, seeking, and closing. Supports read/write access, preserve/overwrite/append modes, optional creation, move-only ownership, and automatic cleanup.
- **`nexos::Pipe`** — owns an unnamed pipe and exposes its read and write descriptors for inter-process communication.
- **`nexos::Process`** — creates child processes and manages their lifecycle using `fork()`, `execvp()`, and `waitpid()`.
- **Custom errors** — `FileError`, `ProcessError`, and `PipeError` preserve system error information and provide contextual messages.
- **`nex`** — an educational mini shell supporting external commands, `cd`, `help`, `exit`, simple quoting, and pipelines.
- **`nexps`** — a Linux process monitor that reads `/proc` and displays PID, PPID, state, resident memory, CPU ticks, and command name.

## Requirements

- Linux recommended; other Unix-like systems may support the library.
- C++20 compiler (GCC 10+, Clang 12+, or equivalent).
- CMake 3.20 or newer.

`nexps` specifically requires Linux `/proc`. The no-access `File` mode uses `O_PATH` where available. The project is primarily targeted at Linux.

## Build and test

Run from the project root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

The first command configures the project, the second compiles it, and the third runs the registered tests. No external test framework or network download is required by the documented build.

## Run

Start the shell:

```sh
./build/nex
```

Example session:

```text
nex$ help
nex$ pwd
nex$ ls -l | grep cpp | wc -l
nex$ ps aux | grep bash
nex$ exit
```

`pwd`, `ls`, `grep`, `wc`, and `ps` are external programs, not Nexos built-ins. They are located using the caller's `PATH` through `execvp()`.

One-time process snapshot:

```sh
./build/nexps --once
```

Live process monitor (refreshes approximately every second):

```sh
./build/nexps
```

Press `Ctrl+C` to stop the live monitor.

## Library example

```cpp
#include <nexos/file.hpp>
#include <iostream>

int main() {
    nexos::File file("notes.txt", true, true,
                     nexos::WriteMode::Overwrite, true);
    file.write("Hello from Nexos!\\n");
    file.seek(0, SEEK_SET);
    std::cout << file.read_all();
}
```

The example selects read access, write access, overwrite mode, and file creation. Consult `include/nexos/file.hpp` for exact overloads and defaults.

## Project structure

```text
Nexos/
├── include/nexos/       # Public library headers
├── src/                 # Library implementations
├── apps/nex/            # Mini shell
├── apps/nexps/          # Linux /proc process monitor
├── tests/               # Automated tests
├── docs/                # Design and usage documentation
├── CMakeLists.txt
└── README.md
```

## Documentation

- [Architecture](docs/architecture.md) — system calls, ownership, RAII, pipes, and process pipelines.
- [Command reference](docs/command-reference.md) — shell commands, quoting, pipelines, and process monitor usage.
- [API guide](docs/api-reference.md) — library abstractions, usage patterns, and error handling.
- [Testing](docs/testing.md) — build commands and recorded test coverage.
- [Troubleshooting](docs/troubleshooting.md) — common build, runtime, and platform issues.

## Design and ownership

`File`, `Pipe`, and `Process` are non-copyable to prevent accidental duplication of ownership. They are movable so ownership can be transferred safely. Destructors release resources.

The `Process` destructor waits for its child if the caller has not already waited. This avoids leaving an unreaped child, but destroying a live `Process` can block until that child exits.

`File::read(count)` reads at most `count` bytes; a successful read may return fewer. `read_all()` reads from the current offset until EOF. `WriteMode::Preserve` leaves existing data untouched and writes at the current offset, `Overwrite` truncates on open, and `Append` uses `O_APPEND`.

## Limitations

The shell is intentionally compact, not a full POSIX shell. It supports command pipelines and simple quoting, but does not implement shell expansions, globbing, job control, or shell-style I/O redirection.
