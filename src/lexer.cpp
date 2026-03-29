#include "lexer.hpp"
#include <cctype>

Lexer::Lexer(std::string source) : m_source(std::move(source)) {
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) break;

        char c = current();

        if (std::isdigit(c) || c == '.') {
            tokens.push_back(readNumber());
            continue;
        }

        switch (c) {
            case '+': tokens.emplace_back(TokenType::PLUS, "+");
                advance();
                break;
            case '-': tokens.emplace_back(TokenType::MINUS, "-");
                advance();
                break;
            case '*': tokens.emplace_back(TokenType::STAR, "*");
                advance();
                break;
            case '/': tokens.emplace_back(TokenType::SLASH, "/");
                advance();
                break;
            case '(': tokens.emplace_back(TokenType::LPAREN, "(");
                advance();
                break;
            case ')': tokens.emplace_back(TokenType::RPAREN, ")");
                advance();
                break;
            default:
                throw LexerError(std::string("unexpected character '") + c + "'", m_pos);
        }
    }

    tokens.emplace_back(TokenType::END, "");
    return tokens;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(current())) advance();
}

Token Lexer::readNumber() {
    size_t start = m_pos;
    bool hasDot = false;

    while (!isAtEnd() && (std::isdigit(current()) || current() == '.')) {
        if (current() == '.') {
            if (hasDot) throw LexerError("invalid number: multiple decimal points", m_pos);
            hasDot = true;
        }
        advance();
    }

    std::string lexeme = m_source.substr(start, m_pos - start);
    return Token(TokenType::NUMBER, lexeme, std::stod(lexeme));
}
