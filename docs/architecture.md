# Nexos Architecture

## Overview

Nexos has three layers:

1. **Public interface** (`include/nexos`) — C++ classes and types consumed by applications.
2. **System-call implementation** (`src`) — maps the public interface to Unix/Linux primitives.
3. **Demonstration applications** (`apps`) — a pipeline-capable shell and a `/proc` process monitor.

## Resource ownership

`File` owns one file descriptor returned by `open()`. `Pipe` owns two descriptors returned by `pipe2()`. `Process` owns a child-process handle (PID) until it has been waited for. These types delete copy operations and implement move operations so a resource has one C++ owner.

## File I/O

`File` maps to `open`, `read`, `write`, `lseek`, and `close`. `WriteMode` controls truncation/appending independently of access permissions. `O_CLOEXEC` prevents descriptors from unintentionally leaking through `exec`.

## Process management

`Process::spawn` forks. The child optionally redirects standard input/output with `dup2`, closes supplied inherited pipe descriptors, and calls `execvp`. The parent receives a move-only `Process` handle and calls `wait()` to collect the child's status.

## Inter-process communication

`Pipe` wraps an unnamed byte-stream pipe. The shell creates one pipe between each adjacent pair of commands. It connects the producer's stdout to the pipe write end and the consumer's stdin to the pipe read end. Unused descriptors are closed so readers can observe EOF.

## Process monitoring

`nexps` enumerates numeric directories in `/proc`, parses `/proc/<pid>/stat`, and reports process ID, parent ID, state, resident set size, and accumulated user/system CPU ticks.
