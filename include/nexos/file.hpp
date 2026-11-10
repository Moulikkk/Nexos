#pragma once

#include <cstddef>
#include <string>
#include <sys/types.h>

namespace nexos {

enum class WriteMode {
    Preserve,  // Keep existing data; writes begin at the current file offset.
    Overwrite, // Truncate the file when it is opened.
    Append     // Every write is directed to the end of the file.
};

class File {
public:
    explicit File(const std::string& path,
                  bool readable = false,
                  bool writable = false,
                  WriteMode mode = WriteMode::Preserve,
                  bool create = false,
                  unsigned int permissions = 0644);
    ~File() noexcept;

    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;

    // Reads up to count bytes from the current offset. An empty result means EOF.
    std::string read(std::size_t count);
    // Reads from the current offset until EOF.
    std::string read_all();
    // Writes all bytes, retrying interrupted and partial writes.
    void write(const std::string& data);
    // Changes the current file offset and returns the resulting offset.
    off_t seek(off_t offset, int whence);

    int native_handle() const noexcept { return fd_; }
    bool is_open() const noexcept { return fd_ >= 0; }
    const std::string& path() const noexcept { return path_; }

private:
    void close_noexcept() noexcept;
    void require_open() const;
    int fd_ = -1;
    bool readable_ = false;
    bool writable_ = false;
    std::string path_;
};

} // namespace nexos
