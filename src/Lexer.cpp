#include "quark/Lexer.h"
#include "quark/Error.h"
#include <cctype>   // for isspace, isalpha, isdigit, isalnum
#include <string>

// Constructor
Lexer::Lexer(const std::string& source) : source(source), curr_pos(0), curr_line(1) {}

namespace {
    bool isIdentifierStart(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    bool isIdentifierPart(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    // Bytes in the UTF-8 character starting at `pos`. Anything that is not a
    // well-formed multi-byte sequence counts as a single byte.
    int utf8Length(const std::string& source, int pos) {
        const int size = static_cast<int>(source.size());
        const unsigned char lead = static_cast<unsigned char>(source[pos]);

        int length = 1;
        if ((lead & 0xE0) == 0xC0) {
            length = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
        }

        if (pos + length > size) {
            return 1;
        }

        for (int i = 1; i < length; i++) {
            const unsigned char next = static_cast<unsigned char>(source[pos + i]);
            if ((next & 0xC0) != 0x80) {
                return 1;
            }
        }

        return length;
    }

    // The message for a character Quark has no use for. A non-ASCII character
    // is several bytes long and is quoted whole; a byte that cannot be shown
    // at all (a control character, or broken UTF-8) is given in hex.
    std::string describeUnexpected(const std::string& source, int pos, int length) {
        const unsigned char byte = static_cast<unsigned char>(source[pos]);

        if (length == 1 && (byte < 0x20 || byte >= 0x7F)) {
            const char* digits = "0123456789ABCDEF";
            std::string hex = "0x";
            hex += digits[byte >> 4];
            hex += digits[byte & 0x0F];

            return "Unexpected byte: " + hex;
        }

        return "Unexpected character: '" + source.substr(pos, length) + "'";
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
            // Swallow the rest of the word so the message shows all of it.
            if (curr_pos < size && isIdentifierStart(source[curr_pos])) {
                while (curr_pos < size && isIdentifierPart(source[curr_pos])) {
                    curr_pos++;
                }

                throw CompileError(
                    curr_line,
                    "Malformed number: " + source.substr(start, curr_pos - start)
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
            default: {
                const int length = utf8Length(source, curr_pos);

                throw CompileError(
                    curr_line,
                    describeUnexpected(source, curr_pos, length)
                );
            }
        }
        curr_pos++;
    }

    // end-of-file
    tokens.push_back({TokenType::END_OF_FILE, "", curr_line});

    return tokens;
}
