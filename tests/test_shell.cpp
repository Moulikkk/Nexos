#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

struct Result {
    int exit_code;
    std::string output;
};

Result run_shell(const std::string& executable, const std::string& input) {
    char input_name[] = "/tmp/nex_input_XXXXXX";
    char output_name[] = "/tmp/nex_output_XXXXXX";

    int input_fd = mkstemp(input_name);
    int output_fd = mkstemp(output_name);

    if (input_fd < 0 || output_fd < 0) {
        throw std::runtime_error("Could not create temporary test files");
    }

    close(input_fd);
    close(output_fd);

    {
        std::ofstream file(input_name);
        file << input;
    }

    pid_t pid = fork();

    if (pid < 0) {
        unlink(input_name);
        unlink(output_name);
        throw std::runtime_error("fork failed");
    }

    if (pid == 0) {
        int in = open(input_name, O_RDONLY);
        int out = open(output_name, O_WRONLY | O_TRUNC);

        if (in < 0 || out < 0) _exit(126);

        dup2(in, STDIN_FILENO);
        dup2(out, STDOUT_FILENO);
        dup2(out, STDERR_FILENO);

        close(in);
        close(out);

        execl(executable.c_str(), executable.c_str(), nullptr);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            unlink(input_name);
            unlink(output_name);
            throw std::runtime_error("waitpid failed");
        }
    }

    std::ifstream file(output_name);
    std::string output(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    unlink(input_name);
    unlink(output_name);

    return {
        WIFEXITED(status) ? WEXITSTATUS(status) : -1,
        output
    };
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: nex_shell_tests <path-to-nex>\n";
        return 1;
    }

    const std::string shell = argv[1];
    int passed = 0;
    int failed = 0;

    auto test = [&](const std::string& name, auto check) {
        try {
            if (check()) {
                ++passed;
                std::cout << "[PASS] " << name << '\n';
            } else {
                ++failed;
                std::cout << "[FAIL] " << name << '\n';
            }
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "[FAIL] " << name << ": " << e.what() << '\n';
        }
    };

    test("Shell starts and exits", [&] {
        auto r = run_shell(shell, "exit\n");
        return r.exit_code == 0 &&
               r.output.find("Nexos Shell") != std::string::npos;
    });

    test("Help built-in", [&] {
        auto r = run_shell(shell, "help\nexit\n");
        return r.output.find("Built-ins: cd [dir], help, exit") !=
               std::string::npos;
    });

    test("Runs an ordinary command", [&] {
        auto r = run_shell(shell, "echo NEXOS_TEST\nexit\n");
        return r.output.find("NEXOS_TEST") != std::string::npos;
    });

    test("Passes command arguments", [&] {
        auto r = run_shell(shell, "printf '%s' hello\nexit\n");
        return r.output.find("hello") != std::string::npos;
    });

    test("Handles quoted arguments", [&] {
        auto r = run_shell(shell,
            "printf '%s\\n' \"hello world\"\nexit\n");
        return r.output.find("hello world") != std::string::npos;
    });

    test("Connects commands using a pipeline", [&] {
        auto r = run_shell(shell,
            "printf 'hello\\n' | wc -c\nexit\n");
        return r.output.find("6") != std::string::npos;
    });

    test("Rejects an empty pipeline command", [&] {
        auto r = run_shell(shell, "echo hello |\nexit\n");
        return r.output.find("invalid empty command in pipeline") !=
               std::string::npos;
    });

    std::cout << "\nShell test cases passed: " << passed << "/"
              << passed + failed << '\n';
    std::cout << "Failures: " << failed << '\n';

    return failed == 0 ? 0 : 1;
}
