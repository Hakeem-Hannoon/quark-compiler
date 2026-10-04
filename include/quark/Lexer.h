#pragma once

#include <string>
#include <vector>
#include "Error.h"
#include "Token.h"

class Lexer {
private:
    std::string source;
    int curr_pos = 0;
    int curr_line = 1;
    int curr_column = 1;
    std::vector<CompileError> found_errors;

    void advance();

public:
    Lexer(const std::string& source);

    // Always returns a complete, EOF-terminated stream. Text that is not valid
    // Quark becomes an INVALID token and an entry in errors(), and lexing
    // carries on, so one run reports every such problem in the file.
    std::vector<Token> tokenize();

    const std::vector<CompileError>& errors() const { return found_errors; }
};
