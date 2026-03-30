#pragma once
#include "token.hpp"
#include <string>
#include <vector>
#include <stdexcept>

class LexerError : public std::runtime_error {
public:
    explicit LexerError(const std::string &msg, size_t pos)
        : std::runtime_error("Lexer error at position " +
                             std::to_string(pos) + ": " + msg) {
    }
};

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    std::string m_source;
    size_t m_pos{0};

    bool isAtEnd() const { return m_pos >= m_source.size(); }
    char current() const { return m_source[m_pos]; }
    char advance() { return m_source[m_pos++]; }

    void skipWhitespace();

    Token readNumber();
    Token readIdentifier();
};
