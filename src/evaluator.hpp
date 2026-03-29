#pragma once
#include "ast.hpp"
#include <stdexcept>

class EvaluationError : public std::runtime_error {
public:
    explicit EvaluationError(const std::string &msg)
        : std::runtime_error("Evaluation error: " + msg) {
    }
};

class Evaluator : public ASTVisitor {
public:
    double evaluate(const ASTNode &root);

    double visit(const NumberNode &node) override;

    double visit(const BinaryOpNode &node) override;

    double visit(const UnaryOpNode &node) override;
};
