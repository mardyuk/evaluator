#include "parser.hpp"

Parser::Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {
}

ASTNodePtr Parser::parse() {
    ASTNodePtr root = parseExpression();
    if (!isAtEnd()) throw ParserError("unexpected token after expression", current());
    return root;
}

const Token &Parser::advance() {
    if (!isAtEnd()) ++m_pos;
    return previous();
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (TokenType t: types) {
        if (current().type == t) {
            advance();
            return true;
        }
    }
    return false;
}

const Token &Parser::expect(TokenType type, const std::string &msg) {
    if (current().type != type) throw ParserError(msg, current());
    return advance();
}

ASTNodePtr Parser::parseExpression() {
    return parseTerm();
}

ASTNodePtr Parser::parseTerm() {
    ASTNodePtr node = parseFactor();
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        TokenType op = previous().type;
        ASTNodePtr right = parseFactor();
        node = std::make_unique<BinaryOpNode>(op, std::move(node), std::move(right));
    }
    return node;
}

ASTNodePtr Parser::parseFactor() {
    ASTNodePtr node = parseUnary();
    while (match({TokenType::STAR, TokenType::SLASH})) {
        TokenType op = previous().type;
        ASTNodePtr right = parseUnary();
        node = std::make_unique<BinaryOpNode>(op, std::move(node), std::move(right));
    }
    return node;
}

ASTNodePtr Parser::parseUnary() {
    if (match({TokenType::MINUS})) {
        TokenType op = previous().type;
        ASTNodePtr operand = parseUnary();
        return std::make_unique<UnaryOpNode>(op, std::move(operand));
    }
    return parsePrimary();
}

ASTNodePtr Parser::parsePrimary() {
    if (match({TokenType::NUMBER}))
        return std::make_unique<NumberNode>(previous().value);

    if (match({TokenType::IDENTIFIER}))
        return std::make_unique<VariableNode>(previous().lexeme);

    if (match({TokenType::LPAREN})) {
        ASTNodePtr inner = parseExpression();
        expect(TokenType::RPAREN, "expected ')' after expression");
        return inner;
    }

    throw ParserError("expected a number or '('", current());
}
