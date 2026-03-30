#pragma once
#include "ast.hpp"
#include <stdexcept>
#include <map>
#include <string>

class EvaluationError : public std::runtime_error {
public:
    explicit EvaluationError(const std::string &msg)
        : std::runtime_error("Evaluation error: " + msg) {
    }
};

class Evaluator : public ASTVisitor {
public:
    double evaluate(const ASTNode &root);

    void setVariable(const std::string &name, double value);

    double visit(const NumberNode &node) override;

    double visit(const BinaryOpNode &node) override;

    double visit(const UnaryOpNode &node) override;

    double visit(const VariableNode &node) override;

private:
    std::map<std::string, double> m_variables;
};
