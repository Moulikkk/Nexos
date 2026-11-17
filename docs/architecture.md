# Nexos architecture

## 1. System overview

Nexos has a reusable systems library and two demonstration applications.

```text
                 User
                  |
          +-------+-------+
          |               |
       nex shell       nexps monitor
          |               |
    Nexos library       Linux /proc
     /    |    \
  File   Pipe  Process
   |      |      |
 open/   pipe() fork/execvp/
 read/          waitpid()
 write/
 lseek/
 close
```

The library maps operating-system resources to C++ objects. The applications demonstrate how those objects can be combined.

## 2. File descriptors

A file descriptor is a small integer used by a process to refer to an open kernel-managed I/O resource. It may refer to a regular file, pipe, terminal, socket, or another supported object.

| Descriptor | Conventional name | Purpose |
|---:|---|---|
| 0 | Standard input | Input to a process |
| 1 | Standard output | Normal output |
| 2 | Standard error | Error output |

`open()` returns a descriptor on success. `read()`, `write()`, and `lseek()` operate on it. `close()` releases the process's reference to it.

## 3. RAII and ownership

RAII (Resource Acquisition Is Initialization) ties a resource's lifetime to an object's lifetime:

1. Construct an object and acquire the resource.
2. Use the object's methods.
3. Let the object leave scope.
4. Its destructor releases or finalizes the resource.

Nexos uses this approach for file descriptors, pipe descriptors, and child-process lifecycle management.

### Why copying is disabled

If two objects copied the same raw descriptor and both believed they owned it, both destructors could close it. The descriptor number might then be reused for another resource, making accidental double-close especially dangerous. Owning classes are therefore non-copyable.

### Why moving is supported

Move operations transfer ownership instead of duplicating it. The destination becomes responsible for cleanup, while the source no longer owns the resource.

## 4. File abstraction

`nexos::File` wraps the descriptor-based file API.

| System call | Purpose |
|---|---|
| `open()` | Open a path and obtain a descriptor |
| `read()` | Read bytes |
| `write()` | Write bytes |
| `lseek()` | Change the current offset |
| `close()` | Release the descriptor |

Conceptual lifecycle:

```text
construct File -> open -> use descriptor -> destructor -> close
```

Write modes:

- **Preserve:** leave existing data intact and write at the current offset.
- **Overwrite:** truncate existing contents when opening.
- **Append:** use `O_APPEND`, directing writes to the end.

A bounded read requests at most the specified number of bytes. It is not guaranteed to return that many. `read_all()` continues from the current offset until EOF; it does not implicitly rewind first.

## 5. Process abstraction

The usual Unix process lifecycle is:

```text
Parent
  |
  | fork()
  +-------------------------+
  |                         |
  v                         v
Parent continues          Child
                            |
                         execvp()
                            |
                      New program runs
                            |
                         exits
                            |
Parent calls waitpid() <----+
        |
  collect status
```

- `fork()` creates a child process.
- `execvp()` replaces the child process image with the requested executable. Its `p` variant searches `PATH`.
- `waitpid()` lets the parent wait for a child and collect its termination status.

A successful `execvp()` does not return to the old program image. If it fails, the child-side code must handle the error.

The documented `Process` destructor waits if the caller has not already waited. This avoids leaving an unreaped child, but destruction can block while a child is still running.

## 6. Pipes and inter-process communication

An unnamed pipe is a unidirectional byte stream with two descriptors:

```text
writer -- write descriptor --> kernel pipe buffer
                                  |
reader <-- read descriptor -------+
```

Pipes do not preserve message boundaries. Readers receive a stream of bytes.

Important rules:

- A pipe has finite buffering; a writer can block if the buffer fills.
- A reader can block while waiting for data.
- EOF is observed only after all references to the write end are closed and buffered data is consumed.
- Each process should close pipe ends it does not use.

`nexos::Pipe` owns the descriptors created for an unnamed pipe.

## 7. How command pipelines work

For:

```sh
ls -l | grep cpp | wc -l
```

the shell creates pipes between adjacent commands:

```text
[ls -l] --pipe 1--> [grep cpp] --pipe 2--> [wc -l]
```

At a high level, it:

1. Splits the input into pipeline stages while respecting supported quoting.
2. Parses each stage into a command and its arguments.
3. Creates unnamed pipes.
4. Spawns a process for each command.
5. Connects each child's standard input/output to the appropriate pipe ends.
6. Closes pipe descriptors the parent no longer needs.
7. Waits for the child processes and observes their termination.

Closing unused ends is essential. If a process accidentally retains a pipe's write end, a reader may never receive EOF and can hang.

For exact descriptor wiring, consult `apps/nex/main.cpp` and `src/process.cpp` in the source revision being used.

## 8. Error handling

System calls commonly report failure through a return value and set `errno`. Nexos defines `FileError`, `ProcessError`, and `PipeError` to provide contextual messages while preserving the associated system error code.

Typical failure causes include inaccessible paths, permission errors, failed I/O, process-creation errors, missing executables, and pipe-creation errors. Exact constructors and accessors are declared in `include/nexos/error.hpp`.

## 9. Process monitor

`nexps` is a Linux-specific application that reads process metadata from `/proc`.

| Field | Meaning |
|---|---|
| PID | Process identifier |
| PPID | Parent process identifier |
| State | Process state |
| RSS | Resident memory, displayed in KB |
| CPU ticks | CPU time counters |
| Command | Process command name |

`/proc` is a virtual filesystem exposing kernel and process information. It is not an ordinary persistent directory of files.

## 10. Scope

Nexos is an educational abstraction layer, not a complete POSIX shell or general-purpose process supervisor. The shell supports external commands, simple quoting, and pipelines, but not shell expansion, globbing, job control, or shell-style redirection.
