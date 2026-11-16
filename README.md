# Nexos — A Unix Systems Abstraction Framework

Nexos is a C++20 library that wraps core Unix/Linux operating-system primitives in small RAII-based C++ abstractions. It demonstrates file descriptors, process creation and synchronization, unnamed pipes, and command pipelines.

## Features

- **File I/O (`nexos::File`)** — wraps `open`, `read`, `write`, `lseek`, and `close`; supports read/write access, preserve/overwrite/append modes, optional creation, move-only ownership, and automatic cleanup.
- **Process management (`nexos::Process`)** — wraps `fork`, `execvp`, and `waitpid`; supports child process creation and exit-status retrieval.
- **Pipes (`nexos::Pipe`)** — owns an unnamed pipe and exposes its read/write descriptors for parent-child IPC and command pipelines.
- **Custom errors** — `FileError`, `ProcessError`, and `PipeError` preserve the system error code and provide contextual messages.
- **Mini shell (`nex`)** — executes external commands, supports `cd`, `help`, `exit`, quoting, and pipelines such as `ls -l | grep cpp | wc -l`.
- **Process monitor (`nexps`)** — reads Linux `/proc` process statistics and displays PID, PPID, state, resident memory, CPU ticks, and command name.

## Requirements

- Linux (recommended) or another Unix-like system with a C++20 compiler.
- CMake 3.20 or newer.
- GCC 10+, Clang 12+, or another compiler with C++20 support.

`nexps` reads Linux `/proc`, and the no-access `File` mode uses `O_PATH` where available. The project is primarily targeted at Linux.

## Build

From the project root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

No external test framework or network download is required.

## Run

Start the shell:

```bash
./build/nex
```

Try a pipeline:

```text
nex$ ls -l | grep cpp | wc -l
nex$ ps aux | grep bash
nex$ help
nex$ exit
```

Show a one-time process snapshot:

```bash
./build/nexps --once
```

Run the live process monitor (refreshes every second):

```bash
./build/nexps
```

Press Ctrl+C to exit the monitor.

## Library example

```cpp
#include <nexos/file.hpp>
#include <iostream>

int main() {
    nexos::File file("notes.txt", true, true,
                     nexos::WriteMode::Overwrite, true);
    file.write("Hello from Nexos!\n");
    file.seek(0, SEEK_SET);
    std::cout << file.read_all();
}
```

## Project layout

```text
Nexos/
├── include/nexos/       # Public library headers
├── src/                 # Library implementations
├── apps/nex/            # Mini shell
├── apps/nexps/          # /proc process monitor
├── tests/               # Automated tests
├── docs/                # Design and usage notes
├── CMakeLists.txt
└── README.md
```

## Design notes

- `File`, `Pipe`, and `Process` are non-copyable to prevent accidental duplication of ownership.
- They are movable so ownership can be transferred safely.
- Destructors release owned resources. A `Process` destructor waits for its child if the caller has not already waited; this avoids leaving an unreaped child process, but means destroying a live `Process` can block until that child exits.
- `File::read(count)` reads at most `count` bytes; `read_all()` reads from the current offset until EOF.
- `WriteMode::Preserve` leaves existing data untouched and writes at the current offset. `Overwrite` truncates on open. `Append` uses `O_APPEND`.
- `Process::spawn` uses `execvp`, so executable lookup follows the caller's `PATH`.
- The shell is intentionally a compact educational shell, not a full POSIX shell: it supports command pipelines and simple quoting, but not shell expansions, globbing, job control, or I/O redirection syntax.

