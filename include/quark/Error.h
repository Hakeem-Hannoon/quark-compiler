#pragma once

#include <stdexcept>
#include <string>

// A user-facing compile error: something wrong with the .qr source, as opposed
// to a bug in the compiler. Carries the line so the driver can print
// `file.qr:3: message`.
class CompileError : public std::runtime_error {
private:
    int error_line;

public:
    CompileError(int line, const std::string& message)
        : std::runtime_error(message), error_line(line) {}

    int line() const { return error_line; }
};
