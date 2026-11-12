#pragma once

#include <sys/types.h>
#include <string>
#include <vector>

namespace nexos {

struct ProcessResult {
    int exit_code = -1;
    bool exited = false;
    bool signaled = false;
    int signal = 0;
};

class Process {
public:
    // stdin_fd/stdout_fd are borrowed descriptors. extra_close_fds are closed in the child
    // after redirection, which is useful when constructing pipelines.
    static Process spawn(const std::vector<std::string>& arguments,
                         int stdin_fd = -1,
                         int stdout_fd = -1,
                         const std::vector<int>& extra_close_fds = {});

    Process() noexcept = default;
    ~Process() noexcept;
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&& other) noexcept;
    Process& operator=(Process&& other) noexcept;

    ProcessResult wait();
    pid_t pid() const noexcept { return pid_; }
    bool valid() const noexcept { return pid_ > 0; }

private:
    explicit Process(pid_t pid) noexcept : pid_(pid) {}
    void wait_noexcept() noexcept;
    pid_t pid_ = -1;
};

} // namespace nexos
