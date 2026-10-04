#pragma once

#include <stdexcept>
#include <string>

// A user-facing compile error: something wrong with the .qr source, as opposed
// to a bug in the compiler. Carries the position so the driver can print
// `file.qr:3:9: error: message`.
class CompileError : public std::runtime_error {
private:
    int error_line;
    int error_column;

public:
    CompileError(int line, int column, const std::string& message)
        : std::runtime_error(message), error_line(line), error_column(column) {}

    int line() const { return error_line; }
    int column() const { return error_column; }
};
