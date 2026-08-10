#pragma once

#include <string>
#include <vector>
#include "Token.h"

class Lexer {
private:
    std::string source;
    int curr_pos = 0;

public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();
};
