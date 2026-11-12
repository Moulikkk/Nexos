#pragma once

#include <string>

namespace nexos {

class Pipe {
public:
    Pipe();
    ~Pipe() noexcept;

    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    Pipe(Pipe&& other) noexcept;
    Pipe& operator=(Pipe&& other) noexcept;

    int read_fd() const noexcept { return read_fd_; }
    int write_fd() const noexcept { return write_fd_; }
    void close_read() noexcept;
    void close_write() noexcept;
    std::string read_all();
    void write_all(const std::string& data);

private:
    int read_fd_ = -1;
    int write_fd_ = -1;
};

} // namespace nexos
