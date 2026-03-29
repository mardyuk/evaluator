#include "evaluator.hpp"

double Evaluator::evaluate(const ASTNode &root) {
    return root.accept(*this);
}

double Evaluator::visit(const NumberNode &node) {
    return node.value;
}

double Evaluator::visit(const BinaryOpNode &node) {
    double lhs = node.left->accept(*this);
    double rhs = node.right->accept(*this);

    switch (node.op) {
        case TokenType::PLUS: return lhs + rhs;
        case TokenType::MINUS: return lhs - rhs;
        case TokenType::STAR: return lhs * rhs;
        case TokenType::SLASH:
            if (rhs == 0.0) throw EvaluationError("division by zero");
            return lhs / rhs;
        default:
            throw EvaluationError("unknown binary operator");
    }
}

double Evaluator::visit(const UnaryOpNode &node) {
    double val = node.operand->accept(*this);
    switch (node.op) {
        case TokenType::MINUS: return -val;
        default: throw EvaluationError("unknown unary operator");
    }
}
