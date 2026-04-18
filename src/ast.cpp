#include "ast.hpp"

// ─── BinNode ─────────────────────────────────────────────────────────────────

Op BinNode::opCode() const {
    if (_op == "+")   return Op::ADD;
    if (_op == "-")   return Op::SUB;
    if (_op == "*")   return Op::MUL;
    if (_op == "/")   return Op::DIV;
    if (_op == "%")   return Op::MOD;
    if (_op == "**")  return Op::POW;
    if (_op == "//")  return Op::FDIV;
    if (_op == "%/")  return Op::FRACDIV;
    if (_op == "&")   return Op::BAND;
    if (_op == "|")   return Op::BOR;
    if (_op == "^")   return Op::BXOR;
    if (_op == "<<")  return Op::SHL;
    if (_op == ">>")  return Op::SHR;
    if (_op == "==")  return Op::EQ;
    if (_op == "!=")  return Op::NEQ;
    if (_op == "<")   return Op::LT;
    if (_op == ">")   return Op::GT;
    if (_op == "<=")  return Op::LEQ;
    if (_op == ">=")  return Op::GEQ;
    if (_op == "and") return Op::AND;
    if (_op == "or")  return Op::OR;
    return Op::ADD;
}

void BinNode::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "BinOp(" << _op << ")\n";
    std::string np = pfx + (last ? "    " : "│   ");
    _l->dump(np, false);
    _r->dump(np, true);
}

// ─── UnaryNode ───────────────────────────────────────────────────────────────

void UnaryNode::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "Unary(" << _op << ")\n";
    _child->dump(pfx + (last ? "    " : "│   "), true);
}

// ─── TernaryNode ─────────────────────────────────────────────────────────────

void TernaryNode::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "Ternary(? :)\n";
    std::string np = pfx + (last ? "    " : "│   ");
    _cond->dump(np, false);
    _then->dump(np, false);
    _else->dump(np, true);
}

// ─── VarNode ─────────────────────────────────────────────────────────────────

void VarNode::dump(std::string pfx, bool last) const {
    std::string loc = _local
        ? ("local off=" + std::to_string(lOff) + (_hops ? " outer=" + std::to_string(_hops) : ""))
        : ("global addr=" + std::to_string(gAddr));
    std::cout << pfx << (last ? "└── " : "├── ") << "Var(" << loc << ")\n";
}

// ─── ConstNode ───────────────────────────────────────────────────────────────

void ConstNode::dump(std::string pfx, bool last) const {
    const char* name = "?";
    switch (_which) {
        case Op::CONST_PI:  name = "PI";  break;
        case Op::CONST_E:   name = "E";   break;
        case Op::CONST_INF: name = "INF"; break;
        case Op::CONST_MAX: name = "MAX"; break;
        default: break;
    }
    std::cout << pfx << (last ? "└── " : "├── ") << "Const(" << name << ")\n";
}

// ─── CallNode ────────────────────────────────────────────────────────────────

void CallNode::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "Call(" << _name << ")\n";
}

// ─── Block ───────────────────────────────────────────────────────────────────

void Block::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "Block\n";
    std::string np = pfx + (last ? "    " : "│   ");
    for (size_t i = 0; i < _stmts.size(); ++i)
        _stmts[i]->dump(np, i + 1 == _stmts.size());
}

// ─── AssignStmt ──────────────────────────────────────────────────────────────

void AssignStmt::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "Assign\n";
    std::string np = pfx + (last ? "    " : "│   ");
    std::cout << np << "├── " << (_local ? "local off=" + std::to_string(_lOff) : "global addr=" + std::to_string(_gAddr)) << "\n";
    _val->dump(np, true);
}

// ─── IfStmt ──────────────────────────────────────────────────────────────────

void IfStmt::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "If\n";
    std::string np = pfx + (last ? "    " : "│   ");
    _cond->dump(np, false);
    _then->dump(np, _else == nullptr);
    if (_else) _else->dump(np, true);
}

// ─── WhileStmt ───────────────────────────────────────────────────────────────

void WhileStmt::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "While\n";
    std::string np = pfx + (last ? "    " : "│   ");
    _cond->dump(np, false);
    _body->dump(np, true);
}

// ─── ForStmt ─────────────────────────────────────────────────────────────────

void ForStmt::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "For\n";
    std::string np = pfx + (last ? "    " : "│   ");
    if (_init)   _init->dump(np, false);
    if (_cond)   _cond->dump(np, false);
    if (_update) _update->dump(np, false);
    if (_body)   _body->dump(np, true);
}

// ─── PrintStmt ───────────────────────────────────────────────────────────────

void PrintStmt::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "Print\n";
    std::string np = pfx + (last ? "    " : "│   ");
    for (size_t i = 0; i < _exprs.size(); ++i)
        _exprs[i]->dump(np, i + 1 == _exprs.size());
}

// ─── FnDefStmt ───────────────────────────────────────────────────────────────

void FnDefStmt::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ")
              << "FnDef(" << _name << (_isVoid ? " void" : "") << ")\n";
    std::string np = pfx + (last ? "    " : "│   ");
    std::cout << np << "├── params:";
    for (const auto& p : _params) std::cout << " " << p;
    std::cout << "\n";
    if (_body) _body->dump(np, true);
}

// ─── ReturnStmt ──────────────────────────────────────────────────────────────

void ReturnStmt::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "Return\n";
    if (_expr) _expr->dump(pfx + (last ? "    " : "│   "), true);
}

// ─── SwitchStmt ──────────────────────────────────────────────────────────────

void SwitchStmt::dump(std::string pfx, bool last) const {
    std::cout << pfx << (last ? "└── " : "├── ") << "Switch\n";
    std::string np = pfx + (last ? "    " : "│   ");
    _expr->dump(np, false);
    for (size_t i = 0; i < _cases.size(); ++i) {
        bool isLast = (i + 1 == _cases.size()) && !_default;
        std::cout << np << (isLast ? "└── " : "├── ") << "case " << i << "\n";
        _cases[i].body->dump(np + (isLast ? "    " : "│   "), true);
    }
    if (_default) {
        std::cout << np << "└── default\n";
        _default->dump(np + "    ", true);
    }
}
