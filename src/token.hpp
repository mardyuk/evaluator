#pragma once
#include <string>

enum class TokenType {
    NUMBER,
    PLUS,
    MINUS,
    STAR,
    SLASH,
    LPAREN,
    RPAREN,
    END
};

struct Token {
    TokenType type;
    std::string lexeme;
    double value{};

    Token(TokenType t, std::string lex)
        : type(t), lexeme(std::move(lex)) {
    }

    Token(TokenType t, std::string lex, double val)
        : type(t), lexeme(std::move(lex)), value(val) {
    }
};
