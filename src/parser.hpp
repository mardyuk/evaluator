#pragma once
#include "ast.hpp"
#include "token.hpp"
#include <vector>
#include <stdexcept>

class ParserError : public std::runtime_error {
public:
    explicit ParserError(const std::string &msg, const Token &tok)
        : std::runtime_error("Parser error at '" + tok.lexeme + "': " + msg) {
    }
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    ASTNodePtr parse();

private:
    std::vector<Token> m_tokens;
    size_t m_pos{0};

    const Token &current() const { return m_tokens[m_pos]; }
    const Token &previous() const { return m_tokens[m_pos - 1]; }
    bool isAtEnd() const { return current().type == TokenType::END; }

    const Token &advance();

    bool match(std::initializer_list<TokenType> types);

    const Token &expect(TokenType type, const std::string &msg);

    ASTNodePtr parseExpression();

    ASTNodePtr parseTerm();

    ASTNodePtr parseFactor();

    ASTNodePtr parseUnary();

    ASTNodePtr parsePrimary();
};
