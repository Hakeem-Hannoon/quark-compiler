#include "quark/Lexer.h"
#include "quark/Error.h"
#include <cctype>   // for isspace, isalpha, isdigit, isalnum
#include <string>

// Constructor
Lexer::Lexer(const std::string& source)
    : source(source), curr_pos(0), curr_line(1), curr_column(1) {}

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

// Steps past one byte, keeping the line and column in step with it. The
// trailing bytes of a UTF-8 character do not move the column, so a column is a
// count of characters.
void Lexer::advance() {
    const unsigned char byte = static_cast<unsigned char>(source[curr_pos]);
    curr_pos++;

    if (byte == '\n') {
        curr_line++;
        curr_column = 1;
    } else if ((byte & 0xC0) != 0x80) {
        curr_column++;
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    found_errors.clear();
    const int size = static_cast<int>(source.size());

    while (curr_pos < size) {
        char c = source[curr_pos];

        // 1. Skip whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        // 2. Comments: `// ...` to end of line
        if (c == '/' && curr_pos + 1 < size && source[curr_pos + 1] == '/') {
            while (curr_pos < size && source[curr_pos] != '\n') {
                advance();
            }
            continue;
        }

        // Everything below starts a token (or an error) at this position.
        const int line = curr_line;
        const int column = curr_column;

        // 3. Identifiers and keywords
        if (isIdentifierStart(c)) {
            int start = curr_pos;
            while (curr_pos < size && isIdentifierPart(source[curr_pos])) {
                advance();
            }
            std::string text = source.substr(start, curr_pos - start);

            if (text == "let") {
                tokens.push_back({TokenType::LET, text, line, column});
            } else if (text == "true" || text == "false") {
                tokens.push_back({TokenType::BOOLEAN, text, line, column});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, text, line, column});
            }
            continue;
        }

        // 4. Integer literals
        if (std::isdigit(static_cast<unsigned char>(c))) {
            int start = curr_pos;
            while (curr_pos < size &&
                   std::isdigit(static_cast<unsigned char>(source[curr_pos]))) {
                advance();
            }

            // `12abc` is a malformed literal, not an integer followed by a name.
            // Swallow the rest of the word so the message shows all of it.
            if (curr_pos < size && isIdentifierStart(source[curr_pos])) {
                while (curr_pos < size && isIdentifierPart(source[curr_pos])) {
                    advance();
                }

                std::string text = source.substr(start, curr_pos - start);

                found_errors.push_back(
                    CompileError(line, column, "Malformed number: " + text)
                );
                tokens.push_back({TokenType::INVALID, text, line, column});
                continue;
            }

            std::string text = source.substr(start, curr_pos - start);
            tokens.push_back({TokenType::INTEGER, text, line, column});
            continue;
        }

        // 5. Operators and punctuation
        //
        // `=` and `==` begin the same way, so one character is not enough to
        // tell them apart. Look at the one after it and take the longer token
        // whenever it fits; this rule is known as "maximal munch". `next` is
        // '\0' at the end of the file, which completes nothing.
        const char next = (curr_pos + 1 < size) ? source[curr_pos + 1] : '\0';

        TokenType type = TokenType::INVALID;
        int length = 1;

        // Picks the two-character token if `next` completes it, and otherwise
        // the one-character token that `c` is on its own.
        auto longest = [&](char second, TokenType two, TokenType one) {
            if (next == second) {
                type = two;
                length = 2;
            } else {
                type = one;
            }
        };

        switch (c) {
            case '=': longest('=', TokenType::EQUAL_EQUAL, TokenType::EQUAL); break;
            case '!': longest('=', TokenType::BANG_EQUAL, TokenType::BANG); break;
            case '<': longest('=', TokenType::LESS_EQUAL, TokenType::LESS); break;
            case '>': longest('=', TokenType::GREATER_EQUAL, TokenType::GREATER); break;

            // A single & or | is not an operator in Quark.
            case '&': longest('&', TokenType::AND_AND, TokenType::INVALID); break;
            case '|': longest('|', TokenType::OR_OR, TokenType::INVALID); break;

            case '+': type = TokenType::PLUS; break;
            case '-': type = TokenType::MINUS; break;
            case '*': type = TokenType::STAR; break;
            case '/': type = TokenType::SLASH; break;
            case '(': type = TokenType::LPAREN; break;
            case ')': type = TokenType::RPAREN; break;
            case ',': type = TokenType::COMMA; break;
            case ';': type = TokenType::SEMICOLON; break;

            default: break;
        }

        if (type == TokenType::INVALID) {
            length = utf8Length(source, curr_pos);

            std::string message = describeUnexpected(source, curr_pos, length);

            if (c == '&' || c == '|') {
                message += std::string(" (did you mean '") + c + c + "'?)";
            }

            found_errors.push_back(CompileError(line, column, message));
        }

        tokens.push_back({type, source.substr(curr_pos, length), line, column});

        for (int i = 0; i < length; i++) {
            advance();
        }
    }

    // end-of-file
    tokens.push_back({TokenType::END_OF_FILE, "", curr_line, curr_column});

    return tokens;
}
