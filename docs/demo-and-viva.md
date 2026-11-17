# Demonstration and Viva Notes

## Short project explanation

Nexos is a C++20 Unix systems abstraction framework. It wraps low-level operating-system resources in C++ classes so their lifetimes are managed automatically. Its main abstractions are `File`, `Process`, and `Pipe`. The project also includes a small shell that demonstrates process pipelines and a process monitor that reads Linux `/proc` information.

## What each component demonstrates

| Component | Unix/Linux primitives | Purpose |
|---|---|---|
| `File` | `open`, `read`, `write`, `lseek`, `close` | File-descriptor-based file I/O; used by the shell for `<`, `>`, `>>` redirection |
| `Process` | `fork`, `execvp`, `waitpid` | Create a child process, run a program, collect its status |
| `Pipe` | `pipe2`, `read`, `write`, `close` | Byte-stream communication between processes |
| `nex` | `fork`, `execvp`, `dup2`, pipes, files, `waitpid` | Execute commands and connect their standard streams to pipes and files |
| `nexps` | `/proc/<pid>/stat` | Inspect process state and resource statistics |

## Concepts to explain

### RAII

Resource Acquisition Is Initialization ties an operating-system resource to a C++ object's lifetime. A successful `File` owns a descriptor and closes it in its destructor. `Pipe` closes both pipe ends. `Process` waits for its child if the caller has not already collected its status.

### Why copying is disabled

A raw descriptor or PID is a handle, not a resource that can safely be duplicated by copying its integer value. If two objects both believed they owned the same handle, both could release it. Nexos deletes copy construction/assignment and supports moves to transfer ownership.

### Why move operations exist

A move transfers the handle to a new object and leaves the source empty. This makes ownership explicit while allowing objects to be returned from functions and stored in containers.

### How a pipeline works

For `producer | consumer`, the shell creates an unnamed pipe. It uses `dup2` in the producer child to connect stdout to the pipe's write end, and in the consumer child to connect stdin to the read end. The parent closes its unused ends and waits for both children. Closing unused descriptors is essential: otherwise the consumer may never receive EOF.

### Why `execvp` is used

`execvp` replaces the child process image with the requested program and searches directories listed in `PATH`. It does not create another process; `fork` creates the child, and `execvp` replaces what that child runs.

### How redirection works

For `cmd > out.txt`, the shell opens `out.txt` with `File` (write-only, `Overwrite`, create) in the parent, before starting any child, and passes the descriptor to `Process::spawn` as the child's standard output. In the child, `dup2` places it on descriptor 1, so the program just writes to stdout without knowing a file is behind it. `>>` uses `Append` (`O_APPEND`), and `<` opens read-only and becomes standard input.

The file is opened in the parent so that a bad path becomes a normal `FileError` exception before any process exists, and so `File`'s destructor closes the parent's copy automatically.

## Suggested live demo

1. Build and run automated tests.
2. Start `nex` and execute `pwd`.
3. Run `echo hello world | grep world` to demonstrate a two-command pipeline.
4. Run `ls -l | grep cpp | wc -l` to demonstrate a three-command pipeline.
5. Use `cd /tmp`, then `pwd`, to demonstrate a shell built-in.
6. Demonstrate `File`-based redirection:
   - `echo hello > /tmp/out.txt`, then `cat /tmp/out.txt` (overwrite)
   - `echo again >> /tmp/out.txt`, then `wc -l < /tmp/out.txt` prints 2 (append and input)
   - `sort < /tmp/out.txt | uniq > /tmp/sorted.txt` (redirection plus a pipeline)
   - `cat < /nonexistent` shows a `FileError` message and the shell keeps running
7. Exit with `exit`.
8. Run `./build/nexps --once` to show the process monitor.

To show the real system calls behind redirection, run outside the shell:

```sh
echo 'echo hi > /tmp/o.txt' | strace -f -e trace=openat,dup2,dup3,execve ./build/nex 2>&1 | head -40
```

The trace shows `openat` on the file with `O_CLOEXEC` (the `File` constructor), `dup2`/`dup3` in the child, and `execve`.

## Current scope and limitations

This is an educational mini shell, not a full POSIX shell. It supports simple quoting, pipelines, and basic file redirection (`<`, `>`, `>>`), but not shell variable expansion, globbing, background jobs, command history, standard-error redirection (`2>`), here-documents, or redirection in the middle of a pipeline. The process monitor depends on Linux `/proc`. The framework is intended for Linux systems programming demonstrations.
