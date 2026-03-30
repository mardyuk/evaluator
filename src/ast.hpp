#pragma once
#include "token.hpp"
#include <memory>
#include <string>

struct NumberNode;
struct BinaryOpNode;
struct UnaryOpNode;
struct VariableNode;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual double visit(const NumberNode &node) = 0;

    virtual double visit(const BinaryOpNode &node) = 0;

    virtual double visit(const UnaryOpNode &node) = 0;

    virtual double visit(const VariableNode &node) = 0;
};

struct ASTNode {
    virtual ~ASTNode() = default;

    virtual double accept(ASTVisitor &visitor) const = 0;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

struct NumberNode : ASTNode {
    double value;

    explicit NumberNode(double v) : value(v) {
    }

    double accept(ASTVisitor &v) const override { return v.visit(*this); }
};

struct BinaryOpNode : ASTNode {
    TokenType op;
    ASTNodePtr left;
    ASTNodePtr right;

    BinaryOpNode(TokenType op, ASTNodePtr lhs, ASTNodePtr rhs)
        : op(op), left(std::move(lhs)), right(std::move(rhs)) {
    }

    double accept(ASTVisitor &v) const override { return v.visit(*this); }
};

struct UnaryOpNode : ASTNode {
    TokenType op;
    ASTNodePtr operand;

    UnaryOpNode(TokenType op, ASTNodePtr operand)
        : op(op), operand(std::move(operand)) {
    }

    double accept(ASTVisitor &v) const override { return v.visit(*this); }
};

struct VariableNode : ASTNode {
    std::string name;

    explicit VariableNode(std::string name) : name(std::move(name)) {}

    double accept(ASTVisitor &v) const override { return v.visit(*this); }
};
