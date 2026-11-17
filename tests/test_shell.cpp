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

    test("Output redirection > overwrites a file", [&] {
        unlink("/tmp/nex_redir.txt");
        auto r = run_shell(shell,
            "echo first > /tmp/nex_redir.txt\n"
            "echo second > /tmp/nex_redir.txt\n"
            "cat /tmp/nex_redir.txt\nexit\n");
        unlink("/tmp/nex_redir.txt");
        return r.output.find("second") != std::string::npos &&
               r.output.find("first") == std::string::npos;
    });

    test("Output redirection >> appends to a file", [&] {
        unlink("/tmp/nex_redir.txt");
        auto r = run_shell(shell,
            "echo first > /tmp/nex_redir.txt\n"
            "echo second >> /tmp/nex_redir.txt\n"
            "cat /tmp/nex_redir.txt\nexit\n");
        unlink("/tmp/nex_redir.txt");
        return r.output.find("first") != std::string::npos &&
               r.output.find("second") != std::string::npos;
    });

    test("Input redirection < feeds a file to a command", [&] {
        {
            std::ofstream f("/tmp/nex_redir_in.txt");
            f << "a\nb\nc\n";
        }
        auto r = run_shell(shell, "wc -l < /tmp/nex_redir_in.txt\nexit\n");
        unlink("/tmp/nex_redir_in.txt");
        return r.output.find("3") != std::string::npos;
    });

    test("Redirection combines with a pipeline", [&] {
        unlink("/tmp/nex_redir.txt");
        auto r = run_shell(shell,
            "printf 'b\\na\\nb\\n' | sort | uniq > /tmp/nex_redir.txt\n"
            "cat /tmp/nex_redir.txt\nexit\n");
        unlink("/tmp/nex_redir.txt");
        return r.output.find("a\nb\n") != std::string::npos;
    });

    test("Missing input file reports an error and the shell survives", [&] {
        auto r = run_shell(shell,
            "cat < /nex_no_such_file\necho STILL_ALIVE\nexit\n");
        return r.output.find("open '/nex_no_such_file'") != std::string::npos &&
               r.output.find("STILL_ALIVE") != std::string::npos;
    });

    test("Quoted > is ordinary text, not a redirection", [&] {
        auto r = run_shell(shell, "echo \">\" quoted\nexit\n");
        return r.output.find("> quoted") != std::string::npos;
    });

    test("Missing file name after > is rejected", [&] {
        auto r = run_shell(shell, "echo hi >\nexit\n");
        return r.output.find("expected a file name after '>'") !=
               std::string::npos;
    });

    std::cout << "\nShell test cases passed: " << passed << "/"
              << passed + failed << '\n';
    std::cout << "Failures: " << failed << '\n';

    return failed == 0 ? 0 : 1;
}
