#include <nexos/error.hpp>
#include <nexos/file.hpp>
#include <nexos/nexos.hpp>
#include <nexos/pipe.hpp>
#include <nexos/process.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <unistd.h>

namespace {

int assertions = 0;
int failures = 0;
int passed_cases = 0;

#define CHECK(condition) do { \
    ++assertions; \
    if (!(condition)) { \
        std::cerr << "    FAIL: " << #condition \
                  << " at line " << __LINE__ << '\n'; \
        ++failures; \
    } \
} while (false)

std::string temp_path() {
    char pattern[] = "/tmp/nexos-test-XXXXXX";
    int fd = ::mkstemp(pattern);

    if (fd < 0)
        throw std::runtime_error("mkstemp failed");

    ::close(fd);
    return pattern;
}

template <typename Function>
void run_test(const char* name, Function function) {
    int previous_failures = failures;

    try {
        function();

        if (failures == previous_failures) {
            ++passed_cases;
            std::cout << "  [PASS] " << name << '\n';
        } else {
            std::cout << "  [FAIL] " << name << '\n';
        }
    } catch (const std::exception& e) {
        ++failures;
        std::cerr << "  [ERROR] " << name << ": "
                  << e.what() << '\n';
    }
}

} // namespace

int main() {
    std::cout << "\n========== NEXOS TEST SUITE ==========\n";

    // VERSION

    run_test("Library version", [] {
        CHECK(nexos::version() == "1.0.0");
    });

    // FILE TESTS

    run_test("File creation, writing, reading and seeking", [] {
        const auto path = temp_path();

        {
            nexos::File file(
                path, true, true,
                nexos::WriteMode::Overwrite
            );

            CHECK(file.is_open());

            file.write("hello nexos");

            file.seek(0, SEEK_SET);
            CHECK(file.read(5) == "hello");
        }

        std::remove(path.c_str());
    });

    run_test("File move constructor transfers ownership", [] {
        const auto path = temp_path();

        {
            nexos::File original(
                path, true, true,
                nexos::WriteMode::Overwrite
            );

            original.write("move test");

            nexos::File moved(std::move(original));

            CHECK(!original.is_open());
            CHECK(moved.is_open());

            moved.seek(0, SEEK_SET);
            CHECK(moved.read_all() == "move test");
        }

        std::remove(path.c_str());
    });

    run_test("File append mode", [] {
        const auto path = temp_path();

        {
            nexos::File first(
                path, true, true,
                nexos::WriteMode::Overwrite
            );
            first.write("hello");

            nexos::File second(
                path, true, true,
                nexos::WriteMode::Append
            );
            second.write(" world");
        }

        {
            nexos::File reader(path, true, false);
            CHECK(reader.read_all() == "hello world");
        }

        std::remove(path.c_str());
    });

    run_test("File overwrite truncates old contents", [] {
        const auto path = temp_path();

        {
            nexos::File first(
                path, true, true,
                nexos::WriteMode::Overwrite
            );
            first.write("this is the original content");
        }

        {
            nexos::File second(
                path, true, true,
                nexos::WriteMode::Overwrite
            );
            second.write("new");
        }

        {
            nexos::File reader(path, true, false);
            CHECK(reader.read_all() == "new");
        }

        std::remove(path.c_str());
    });

    run_test("File read-only access", [] {
        const auto path = temp_path();

        {
            nexos::File writer(
                path, true, true,
                nexos::WriteMode::Overwrite
            );
            writer.write("read only test");
        }

        {
            nexos::File reader(path, true, false);
            CHECK(reader.is_open());
            CHECK(reader.read_all() == "read only test");
        }

        std::remove(path.c_str());
    });

    run_test("File invalid path reports an exception", [] {
        const auto path = temp_path();
        std::remove(path.c_str());

        bool caught = false;

        try {
            nexos::File invalid(path + "/missing", true, false);
        } catch (const std::exception&) {
            caught = true;
        }

        CHECK(caught);
    });

    // PIPE TESTS

    run_test("Pipe transfers data correctly", [] {
        nexos::Pipe pipe;

        const std::string message = "pipe message";

        auto written = ::write(
            pipe.write_fd(),
            message.data(),
            message.size()
        );

        CHECK(written == static_cast<ssize_t>(message.size()));

        pipe.close_write();

        CHECK(pipe.read_all() == message);
    });

    run_test("Pipe handles empty input", [] {
        nexos::Pipe pipe;

        pipe.close_write();

        CHECK(pipe.read_all().empty());
    });

    run_test("Pipe handles multiple writes", [] {
        nexos::Pipe pipe;

        const std::string first = "first ";
        const std::string second = "second";
        
        auto a = ::write(
            pipe.write_fd(), first.data(), first.size()
        );

        auto b = ::write(
            pipe.write_fd(), second.data(), second.size()
        );

        CHECK(a == static_cast<ssize_t>(first.size()));
        CHECK(b == static_cast<ssize_t>(second.size()));

        pipe.close_write();

        CHECK(pipe.read_all() == "first second");
    });

    run_test("Pipe transfers a larger message", [] {
        nexos::Pipe pipe;

        const std::string message(1024, 'N');

        auto written = ::write(
            pipe.write_fd(),
            message.data(),
            message.size()
        );

        CHECK(written == static_cast<ssize_t>(message.size()));

        pipe.close_write();

        CHECK(pipe.read_all() == message);
    });

    // PROCESS TESTS

    run_test("Process executes a successful command", [] {
        auto process = nexos::Process::spawn({"/bin/true"});

        const auto result = process.wait();

        CHECK(result.exited);
        CHECK(result.exit_code == 0);
    });

    run_test("Process retrieves a non-zero exit code", [] {
        auto process = nexos::Process::spawn({
            "/bin/sh", "-c", "exit 7"
        });

        const auto result = process.wait();

        CHECK(result.exited);
        CHECK(result.exit_code == 7);
    });

    run_test("Process passes command arguments", [] {
        auto process = nexos::Process::spawn({
            "/bin/sh", "-c", "test \"$1\" = hello",
            "sh", "hello"
        });

        const auto result = process.wait();

        CHECK(result.exited && result.exit_code == 0);
    });

    run_test("Process reports a missing executable", [] {
        bool caught = false;

        try {
            auto process = nexos::Process::spawn({
                "/definitely/not/a/real/program"
            });
        } catch (const nexos::ProcessError&) {
            caught = true;
        }

        CHECK(caught);
    });

    // SUMMARY

    std::cout << "\n========== TEST SUMMARY ==========\n";
    std::cout << "Test cases passed: " << passed_cases << "/"
              << 15 << '\n';
    std::cout << "Assertions executed: " << assertions << '\n';
    std::cout << "Failures: " << failures << '\n';

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED!\n";
        return EXIT_SUCCESS;
    }

    std::cout << "\nSOME TESTS FAILED.\n";
    return EXIT_FAILURE;
}
