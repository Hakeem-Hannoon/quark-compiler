#include "quark/Lexer.h"
#include "quark/Error.h"
#include <cctype>   // for isspace, isalpha, isdigit, isalnum

// Constructor
Lexer::Lexer(const std::string& source) : source(source), curr_pos(0), curr_line(1) {}

namespace {
    bool isIdentifierStart(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    bool isIdentifierPart(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    const int size = static_cast<int>(source.size());

    while (curr_pos < size) {
        char c = source[curr_pos];

        // 1. Skip whitespace, counting lines as we go
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (c == '\n') {
                curr_line++;
            }
            curr_pos++;
            continue;
        }

        // 2. Comments: `// ...` to end of line
        if (c == '/' && curr_pos + 1 < size && source[curr_pos + 1] == '/') {
            while (curr_pos < size && source[curr_pos] != '\n') {
                curr_pos++;
            }
            continue;
        }

        // 3. Identifiers and the keyword "let"
        if (isIdentifierStart(c)) {
            int start = curr_pos;
            while (curr_pos < size && isIdentifierPart(source[curr_pos])) {
                curr_pos++;
            }
            std::string text = source.substr(start, curr_pos - start);

            if (text == "let") {
                tokens.push_back({TokenType::LET, text, curr_line});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, text, curr_line});
            }
            continue;
        }

        // 4. Integer literals
        if (std::isdigit(static_cast<unsigned char>(c))) {
            int start = curr_pos;
            while (curr_pos < size &&
                   std::isdigit(static_cast<unsigned char>(source[curr_pos]))) {
                curr_pos++;
            }

            // `12abc` is a malformed literal, not an integer followed by a name.
            if (curr_pos < size && isIdentifierStart(source[curr_pos])) {
                throw CompileError(
                    curr_line,
                    "Malformed number: " + source.substr(start, curr_pos - start + 1)
                );
            }

            std::string text = source.substr(start, curr_pos - start);
            tokens.push_back({TokenType::INTEGER, text, curr_line});
            continue;
        }

        // 5. Single-character tokens
        switch (c) {
            case '=':
                tokens.push_back({TokenType::EQUAL, "=", curr_line});
                break;
            case '+':
                tokens.push_back({TokenType::PLUS, "+", curr_line});
                break;
            case '-':
                tokens.push_back({TokenType::MINUS, "-", curr_line});
                break;
            case '*':
                tokens.push_back({TokenType::STAR, "*", curr_line});
                break;
            case '/':
                tokens.push_back({TokenType::SLASH, "/", curr_line});
                break;
            case '(':
                tokens.push_back({TokenType::LPAREN, "(", curr_line});
                break;
            case ')':
                tokens.push_back({TokenType::RPAREN, ")", curr_line});
                break;
            case ',':
                tokens.push_back({TokenType::COMMA, ",", curr_line});
                break;
            case ';':
                tokens.push_back({TokenType::SEMICOLON, ";", curr_line});
                break;
            default:
                throw CompileError(
                    curr_line,
                    std::string("Unexpected character: '") + c + "'"
                );
        }
        curr_pos++;
    }

    // end-of-file
    tokens.push_back({TokenType::END_OF_FILE, "", curr_line});

    return tokens;
}
