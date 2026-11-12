#include <nexos/process.hpp>
#include <nexos/error.hpp>

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

namespace nexos {

Process Process::spawn(const std::vector<std::string>& arguments,
                      int stdin_fd, int stdout_fd,
                      const std::vector<int>& extra_close_fds) {
    if (arguments.empty() || arguments.front().empty()) {
        throw std::invalid_argument("Process::spawn requires a program name");
    }

    // This close-on-exec pipe lets the parent distinguish a successful exec from
    // an execvp failure in the child. Successful exec closes the write end for us.
    int error_pipe[2];
    if (::pipe2(error_pipe, O_CLOEXEC) == -1) {
        const int saved_errno = errno;
        throw ProcessError(saved_errno, "create exec-status pipe");
    }

    pid_t child = ::fork();
    if (child < 0) {
        const int saved_errno = errno;
        ::close(error_pipe[0]);
        ::close(error_pipe[1]);
        throw ProcessError(saved_errno, "fork");
    }

    if (child == 0) {
        ::close(error_pipe[0]);
        auto report_child_error = [&](int error_number) {
            const char* bytes = reinterpret_cast<const char*>(&error_number);
            std::size_t remaining = sizeof(error_number);
            while (remaining > 0) {
                ssize_t n = ::write(error_pipe[1], bytes + sizeof(error_number) - remaining, remaining);
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) break;
                remaining -= static_cast<std::size_t>(n);
            }
            _exit(126);
        };

        if (stdin_fd >= 0 && ::dup2(stdin_fd, STDIN_FILENO) == -1) report_child_error(errno);
        if (stdout_fd >= 0 && ::dup2(stdout_fd, STDOUT_FILENO) == -1) report_child_error(errno);

        for (int fd : extra_close_fds) {
            if (fd >= 0 && fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != error_pipe[1]) ::close(fd);
        }
        if (stdin_fd > STDERR_FILENO && stdin_fd != error_pipe[1]) ::close(stdin_fd);
        if (stdout_fd > STDERR_FILENO && stdout_fd != stdin_fd && stdout_fd != error_pipe[1]) ::close(stdout_fd);

        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1);
        for (const auto& arg : arguments) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        report_child_error(errno);
    }

    ::close(error_pipe[1]);
    int child_error = 0;
    std::size_t received = 0;
    while (received < sizeof(child_error)) {
        ssize_t n = ::read(error_pipe[0], reinterpret_cast<char*>(&child_error) + received,
                           sizeof(child_error) - received);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) {
            const int saved_errno = errno;
            ::close(error_pipe[0]);
            int status = 0;
            while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            throw ProcessError(saved_errno, "read exec status");
        }
        if (n == 0) break; // CLOEXEC closed the pipe: exec succeeded.
        received += static_cast<std::size_t>(n);
    }
    ::close(error_pipe[0]);

    if (received != 0) {
        int status = 0;
        while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
        if (received != sizeof(child_error)) child_error = EIO;
        throw ProcessError(child_error, "exec '" + arguments.front() + "'");
    }

    return Process(child);
}

Process::~Process() noexcept { wait_noexcept(); }

Process::Process(Process&& other) noexcept
    : pid_(std::exchange(other.pid_, -1)) {}

Process& Process::operator=(Process&& other) noexcept {
    if (this != &other) {
        wait_noexcept();
        pid_ = std::exchange(other.pid_, -1);
    }
    return *this;
}

ProcessResult Process::wait() {
    if (pid_ <= 0) throw std::logic_error("Process is not waitable");
    int status = 0;
    pid_t result;
    do { result = ::waitpid(pid_, &status, 0); } while (result < 0 && errno == EINTR);
    if (result < 0) {
        const int saved_errno = errno;
        throw ProcessError(saved_errno, "waitpid");
    }
    pid_ = -1;
    ProcessResult outcome;
    if (WIFEXITED(status)) {
        outcome.exited = true;
        outcome.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        outcome.signaled = true;
        outcome.signal = WTERMSIG(status);
        outcome.exit_code = 128 + outcome.signal;
    }
    return outcome;
}

void Process::wait_noexcept() noexcept {
    if (pid_ > 0) {
        int status = 0;
        while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {}
        pid_ = -1;
    }
}

} // namespace nexos
