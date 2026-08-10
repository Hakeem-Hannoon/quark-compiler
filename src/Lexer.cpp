#include "Lexer.h"
#include <cctype>   // for isspace, isalpha, isdigit, isalnum

// Constructor
Lexer::Lexer(const std::string& source) : source(source), curr_pos(0) {}

// The tokenize method
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (curr_pos < static_cast<int>(source.size())) {
        char c = source[curr_pos];

        // 1. Skip whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            curr_pos++;
            continue;
        }

        // 2. Identifiers and the keyword "let"
        if (std::isalpha(static_cast<unsigned char>(c))) {
            int start = curr_pos;
            while (curr_pos < static_cast<int>(source.size()) &&
                   std::isalnum(static_cast<unsigned char>(source[curr_pos]))) {
                curr_pos++;
            }
            std::string text = source.substr(start, curr_pos - start);

            if (text == "let") {
                tokens.push_back({TokenType::LET, text});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, text});
            }
            continue;
        }

        // 3. Integer literals
        if (std::isdigit(static_cast<unsigned char>(c))) {
            int start = curr_pos;
            while (curr_pos < static_cast<int>(source.size()) &&
                   std::isdigit(static_cast<unsigned char>(source[curr_pos]))) {
                curr_pos++;
            }
            std::string text = source.substr(start, curr_pos - start);
            tokens.push_back({TokenType::INTEGER, text});
            continue;
        }

        // 4. Single-character tokens
        switch (c) {
            case '=':
                tokens.push_back({TokenType::EQUAL, "="});
                break;
            case '+':
                tokens.push_back({TokenType::PLUS, "+"});
                break;
            case '-':
                tokens.push_back({TokenType::MINUS, "-"});
                break;
            case '*':
                tokens.push_back({TokenType::STAR, "*"});
                break;
            case '/':
                tokens.push_back({TokenType::SLASH, "/"});
                break;
            case '(':
                tokens.push_back({TokenType::LPAREN, "("});
                break;
            case ')':
                tokens.push_back({TokenType::RPAREN, ")"});
                break;
            case ';':
                tokens.push_back({TokenType::SEMICOLON, ";"});
                break;
            default:
                // Unknown character – just skip it for now
                break;
        }
        curr_pos++;
    }

    // Always add the end-of-file token
    tokens.push_back({TokenType::END_OF_FILE, ""});

    return tokens;
}