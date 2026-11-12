#include <nexos/pipe.hpp>
#include <nexos/error.hpp>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <utility>
#include <stdexcept>

namespace nexos {

Pipe::Pipe() {
    int descriptors[2];
#if defined(__linux__)
    if (::pipe2(descriptors, O_CLOEXEC) == -1) {
#else
    if (::pipe(descriptors) == -1) {
#endif
        const int saved_errno = errno;
        throw PipeError(saved_errno, "create pipe");
    }
    read_fd_ = descriptors[0];
    write_fd_ = descriptors[1];
#if !defined(__linux__)
    ::fcntl(read_fd_, F_SETFD, FD_CLOEXEC);
    ::fcntl(write_fd_, F_SETFD, FD_CLOEXEC);
#endif
}

Pipe::~Pipe() noexcept { close_read(); close_write(); }

Pipe::Pipe(Pipe&& other) noexcept
    : read_fd_(std::exchange(other.read_fd_, -1)),
      write_fd_(std::exchange(other.write_fd_, -1)) {}

Pipe& Pipe::operator=(Pipe&& other) noexcept {
    if (this != &other) {
        close_read();
        close_write();
        read_fd_ = std::exchange(other.read_fd_, -1);
        write_fd_ = std::exchange(other.write_fd_, -1);
    }
    return *this;
}

void Pipe::close_read() noexcept {
    if (read_fd_ >= 0) { ::close(read_fd_); read_fd_ = -1; }
}
void Pipe::close_write() noexcept {
    if (write_fd_ >= 0) { ::close(write_fd_); write_fd_ = -1; }
}

std::string Pipe::read_all() {
    if (read_fd_ < 0) throw std::logic_error("Pipe read end is closed");
    std::string result;
    char buffer[4096];
    for (;;) {
        ssize_t n = ::read(read_fd_, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { const int e = errno; throw PipeError(e, "read pipe"); }
        if (n == 0) break;
        result.append(buffer, static_cast<std::size_t>(n));
    }
    return result;
}

void Pipe::write_all(const std::string& data) {
    if (write_fd_ < 0) throw std::logic_error("Pipe write end is closed");
    std::size_t offset = 0;
    while (offset < data.size()) {
        ssize_t n = ::write(write_fd_, data.data() + offset, data.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { const int e = errno; throw PipeError(e, "write pipe"); }
        if (n == 0) throw PipeError(EIO, "pipe write made no progress");
        offset += static_cast<std::size_t>(n);
    }
}

} // namespace nexos
