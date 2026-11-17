# Nexos API guide

This guide explains the public abstractions and their operating-system concepts. For exact signatures, overloads, default arguments, return types, and exception declarations, the public headers in `include/nexos/` are authoritative.

## Headers

Use the headers for the components required by your program:

```cpp
#include <nexos/file.hpp>
#include <nexos/pipe.hpp>
#include <nexos/process.hpp>
#include <nexos/error.hpp>
```

The project also provides an umbrella header, `nexos/nexos.hpp`; consult it to see which declarations it re-exports.

## `nexos::File`

`File` owns an open file descriptor and provides file I/O.

Documented constructor usage:

```cpp
nexos::File file(
    "notes.txt",
    true, true,
    nexos::WriteMode::Overwrite,
    true
);
```

The example selects read access, write access, overwrite mode, and creation. Check `include/nexos/file.hpp` for exact overloads and defaults.

### Write modes

| Mode | Meaning |
|---|---|
| `WriteMode::Preserve` | Existing contents remain; writes begin at the current offset |
| `WriteMode::Overwrite` | Existing contents are truncated on open |
| `WriteMode::Append` | Uses `O_APPEND` behavior |

### Methods and semantics

```cpp
file.write("Hello from Nexos!\n");
file.seek(0, SEEK_SET);
std::string contents = file.read_all();
```

- `write(...)` writes bytes to the descriptor.
- `seek(offset, origin)` changes the current offset using `lseek()` semantics.
- `read(count)` reads at most `count` bytes.
- `read_all()` reads from the current offset until EOF.

A read may return fewer bytes than requested. `read_all()` does not rewind automatically; seek to the beginning first if that is what the caller intends.

`File` is non-copyable and movable. Avoid closing its raw descriptor through another owner while the object still believes it owns it.

## `nexos::Pipe`

`Pipe` owns an unnamed pipe with a read end and a write end. The writer sends bytes; the reader consumes them.

Pipes are byte streams, not message queues. They have finite buffering, and readers generally observe EOF only after every reference to the write end is closed and buffered bytes have been consumed.

Close unused ends in each process. A retained write descriptor can prevent EOF and cause a reader to wait indefinitely.

`Pipe` is non-copyable and movable. Exact constructor and descriptor accessor names are defined in `include/nexos/pipe.hpp`.

## `nexos::Process`

`Process` represents a child process created using the fork/exec model.

1. Prepare the executable and arguments.
2. Spawn a child.
3. The child executes the requested program using `execvp()`.
4. The parent waits using `waitpid()` when appropriate.
5. The parent retrieves the child's termination status.

The shell uses `Process::spawn(...)` to launch commands and supplies input/output descriptors for pipeline stages. See `include/nexos/process.hpp` for the exact signature and descriptor mapping.

Because `execvp()` is used, executable lookup follows `PATH` when a command name does not specify a path.

The documented destructor waits for a child if the caller has not already waited. This prevents an unreaped child but may block while the child is still running. `Process` is non-copyable and movable.

## Error types

Nexos defines:

- `FileError`
- `ProcessError`
- `PipeError`

They provide contextual messages and preserve the associated system error code. Check `include/nexos/error.hpp` for inheritance, constructors, and accessor names.

Example handling pattern:

```cpp
try {
    // Perform Nexos operations.
}
catch (const nexos::FileError& error) {
    std::cerr << error.what() << '\n';
}
```

This example assumes the error type exposes the standard `what()` interface; verify the actual declaration in the public header.

## Complete File example

```cpp
#include <nexos/file.hpp>

#include <iostream>

int main() {
    try {
        nexos::File file(
            "notes.txt",
            true,
            true,
            nexos::WriteMode::Overwrite,
            true
        );

        file.write("Hello from Nexos!\n");
        file.seek(0, SEEK_SET);
        std::cout << file.read_all();
    }
    catch (const std::exception& error) {
        std::cerr << "Nexos example failed: "
                  << error.what() << '\n';
        return 1;
    }
}
```

Link the program against the CMake library target named `nexos`.

## Details to verify for a release

Before treating this as a signature-by-signature reference, compare it with the current headers and add the exact:
- `File` constructors, defaults, and method return types.
- `Pipe` constructor and read/write descriptor accessor names.
- `Process::spawn` arguments and status accessor semantics.
- Error inheritance and error-code accessor names.

This guide deliberately avoids guessing signatures not present in the supplied project description.
