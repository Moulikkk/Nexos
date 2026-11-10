#include <nexos/file.hpp>
#include <nexos/error.hpp>

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace nexos {

File::File(const std::string& path, bool readable, bool writable,
           WriteMode mode, bool create, unsigned int permissions)
    : readable_(readable), writable_(writable), path_(path) {
    if (mode != WriteMode::Preserve && !writable_) {
        throw std::invalid_argument("Overwrite/Append mode requires write access");
    }
    if (!readable_ && !writable_ && create) {
        throw std::invalid_argument("Cannot create a file without read or write access");
    }

    int flags = 0;
    if (readable_ && writable_) flags |= O_RDWR;
    else if (writable_) flags |= O_WRONLY;
    else if (readable_) flags |= O_RDONLY;
#ifdef O_PATH
    else flags |= O_PATH;
#else
    else flags |= O_RDONLY;
#endif

    if (mode == WriteMode::Overwrite) flags |= O_TRUNC;
    if (mode == WriteMode::Append) flags |= O_APPEND;
    if (create) flags |= O_CREAT;
    flags |= O_CLOEXEC;

    fd_ = ::open(path.c_str(), flags, static_cast<mode_t>(permissions));
    if (fd_ == -1) {
        const int saved_errno = errno;
        throw FileError(saved_errno, "open '" + path + "'");
    }
}

File::~File() noexcept { close_noexcept(); }

File::File(File&& other) noexcept
    : fd_(std::exchange(other.fd_, -1)),
      readable_(std::exchange(other.readable_, false)),
      writable_(std::exchange(other.writable_, false)),
      path_(std::move(other.path_)) {}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        close_noexcept();
        fd_ = std::exchange(other.fd_, -1);
        readable_ = std::exchange(other.readable_, false);
        writable_ = std::exchange(other.writable_, false);
        path_ = std::move(other.path_);
    }
    return *this;
}

void File::close_noexcept() noexcept {
    if (fd_ >= 0) {
        // Do not retry close() after EINTR: on Linux the descriptor may already be released.
        ::close(fd_);
        fd_ = -1;
    }
}

void File::require_open() const {
    if (fd_ < 0) throw std::logic_error("File is not open (it may have been moved from)");
}

std::string File::read(std::size_t count) {
    require_open();
    if (!readable_) throw std::logic_error("File was not opened with read access");
    std::string result(count, '\0');
    ssize_t amount;
    do {
        amount = ::read(fd_, result.data(), count);
    } while (amount < 0 && errno == EINTR);
    if (amount < 0) {
        const int saved_errno = errno;
        throw FileError(saved_errno, "read '" + path_ + "'");
    }
    result.resize(static_cast<std::size_t>(amount));
    return result;
}

std::string File::read_all() {
    require_open();
    if (!readable_) throw std::logic_error("File was not opened with read access");
    std::string result;
    char buffer[8192];
    for (;;) {
        ssize_t amount;
        do {
            amount = ::read(fd_, buffer, sizeof(buffer));
        } while (amount < 0 && errno == EINTR);
        if (amount < 0) {
            const int saved_errno = errno;
            throw FileError(saved_errno, "read '" + path_ + "'");
        }
        if (amount == 0) break;
        result.append(buffer, static_cast<std::size_t>(amount));
    }
    return result;
}

void File::write(const std::string& data) {
    require_open();
    if (!writable_) throw std::logic_error("File was not opened with write access");
    std::size_t written = 0;
    while (written < data.size()) {
        ssize_t amount = ::write(fd_, data.data() + written, data.size() - written);
        if (amount < 0 && errno == EINTR) continue;
        if (amount < 0) {
            const int saved_errno = errno;
            throw FileError(saved_errno, "write '" + path_ + "'");
        }
        if (amount == 0) throw FileError(EIO, "write made no progress for '" + path_ + "'");
        written += static_cast<std::size_t>(amount);
    }
}

off_t File::seek(off_t offset, int whence) {
    require_open();
    const off_t result = ::lseek(fd_, offset, whence);
    if (result == static_cast<off_t>(-1)) {
        const int saved_errno = errno;
        throw FileError(saved_errno, "seek '" + path_ + "'");
    }
    return result;
}

} // namespace nexos
