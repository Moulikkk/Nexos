# Testing Nexos

## Configure and build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

For a debug build, use `-DCMAKE_BUILD_TYPE=Debug`.

## Run the automated tests

```sh
ctest --test-dir build --output-on-failure
```

For verbose output:

```sh
ctest --test-dir build --output-on-failure -V
```

CTest runs the tests registered by the project's `CMakeLists.txt`.

## Recorded library coverage

The development test run contained 15 library test groups:

1. Library version.
2. File creation, writing, reading, and seeking.
3. File move-constructor ownership transfer.
4. File append mode.
5. File overwrite truncation.
6. File read-only access.
7. Invalid file path reports an exception.
8. Pipe transfers data correctly.
9. Pipe handles empty input.
10. Pipe handles multiple writes.
11. Pipe transfers a larger message (1024 bytes).
12. Process executes a successful command (`/bin/true`).
13. Process retrieves a non-zero exit code (`/bin/sh -c exit 7`).
14. Process passes command arguments.
15. Process reports a missing executable.

The reported run contained 25 assertions across these groups.

## Recorded shell integration coverage

The shell integration test contained seven groups:

1. Shell starts and exits.
2. Help built-in.
3. Runs an ordinary command.
4. Passes command arguments.
5. Handles quoted arguments.
6. Connects commands using a pipeline.
7. Rejects an empty pipeline command.

## Recorded result

The development run reported:

```text
NexosTests:      passed
NexosShellTests: passed
100% tests passed, 0 tests failed out of 2
```

This is a recorded result from the development environment. Re-run CTest against the exact source revision and target machine before relying on it for a release or submission.

## Manual process monitor check

The process monitor was manually run using:

```sh
./build/nexps --once
./build/nexps
```

The one-time mode printed a snapshot and the live mode refreshed approximately every second. `nexps` was not covered by the two automated CTest executables described above.

## Suggested manual acceptance test

```sh
./build/nex
```

Then enter:

```text
help
pwd
echo "hello from Nexos"
ls -l | grep cpp | wc -l
exit
```

Finally, run:

```sh
./build/nexps --once
```
