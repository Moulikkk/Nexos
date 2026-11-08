#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

namespace nexos {

class SystemError : public std::system_error {
public:
    SystemError(int error_number, const std::string& context)
        : std::system_error(error_number, std::generic_category(), context) {}
};

class FileError : public SystemError {
public:
    FileError(int error_number, const std::string& context)
        : SystemError(error_number, context) {}
};

class ProcessError : public SystemError {
public:
    ProcessError(int error_number, const std::string& context)
        : SystemError(error_number, context) {}
};

class PipeError : public SystemError {
public:
    PipeError(int error_number, const std::string& context)
        : SystemError(error_number, context) {}
};

} // namespace nexos
