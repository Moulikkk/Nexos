# Troubleshooting

## Compiler not found

Check that a C++ compiler is installed and on `PATH`:

```sh
g++ --version
clang++ --version
```

Install a C++20-capable compiler using your Linux distribution's package manager.

## CMake is too old

Nexos requires CMake 3.20 or newer:

```sh
cmake --version
```

Install or select a sufficiently recent version.

## Reconfigure a stale build directory

If compiler or configuration options changed, regenerate the build directory:

```sh
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Only remove `build/` if its generated contents can be recreated.

## Executable not found

Run from the project root:

```sh
./build/nex
./build/nexps --once
```

Check that the build produced the executables:

```sh
ls -l build/nex build/nexps
```

## External command not found

The shell uses `execvp()`, which searches directories in `PATH`.

```sh
command -v grep
command -v wc
```

Install the missing program or ensure its directory is included in `PATH`.

## Pipeline hangs

A pipe reader normally observes EOF only after all open references to the pipe's write end have been closed. If the parent or another child accidentally retains an unused write descriptor, the reader may wait forever.

Also remember that pipe buffers are finite. A writer can block when the buffer fills if no reader is consuming data. Correctly close unused pipe ends in every process.

## File operation fails

Possible causes include a missing path, insufficient permissions, unsupported operation for the descriptor type, filesystem exhaustion, or resource limits. Catch the appropriate Nexos error and inspect its contextual message and system error code.

## Seeking fails

Not all descriptors are seekable. Regular files generally support seeking; pipes and many stream-like descriptors do not.

## `nexps` fails outside Linux

`nexps` reads Linux `/proc` process information. The process monitor is Linux-specific even though the library's core abstractions target Unix-like systems.

## Process destruction blocks

The documented `Process` destructor waits for a child that has not already been waited for. If the child is still running, destruction can block until it exits. Plan the child lifecycle and wait point accordingly.

## Tests fail

Run CTest with verbose output:

```sh
ctest --test-dir build --output-on-failure -V
```

Inspect the failing test output and confirm that the build directory corresponds to the source revision being tested.
