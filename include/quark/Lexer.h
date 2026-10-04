#pragma once

#include <string>
#include <vector>
#include "Token.h"

class Lexer {
private:
    std::string source;
    int curr_pos = 0;
    int curr_line = 1;
    int curr_column = 1;

    void advance();

public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();
};
