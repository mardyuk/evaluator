#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

// ─── OpCodes (instruction set) ───────────────────────────────────────────────
enum class Op : uint8_t {
    // Arithmetic
    ADD, SUB, MUL, DIV, MOD, POW, FDIV, FRACDIV,
    // Bitwise
    BAND, BOR, BXOR, BNOT, SHL, SHR,
    // Comparison
    EQ, NEQ, LT, GT, LEQ, GEQ,
    // Logical
    AND, OR, LNOT,
    // Unary negation
    NEG,
    // Memory
    LOAD_CONST, LOAD_VAR, LOAD_STR, LOAD_NONE,
    STORE_VAR,
    LOAD, STORE,             // frame-pointer relative
    LOAD_OUTER, STORE_OUTER, // enclosing-frame access
    MOV,
    // Control flow
    JMP, JZ,
    // Functions
    CALL, RETURN, PUSH_ARG, LOAD_PARAM,
    // I/O
    PRINT, PRINT_STR,
    // Frame setup (SP/FP adjustment)
    ADDI,
    // Math functions
    SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2,
    SQRT, CBRT, EXP, LOG, LOG10, LOG2, LOG_AB, MATH_POW,
    CEIL, FLOOR, ABS, ROUND, FMOD,
    // Math constants
    CONST_PI, CONST_E, CONST_INF, CONST_MAX,
    // Built-in functions
    INPUT, LENGTH, TYPE, CHR, ORD, BIN, OCT, HEX, DEC,
    // String indexing and random
    STR_GET, RANDOM,
};

// ─── Abstract base ───────────────────────────────────────────────────────────
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void dump(std::string pfx = "", bool last = true) const = 0;
    virtual std::vector<std::shared_ptr<ASTNode>> children() const = 0;
};

class StmtNode : public ASTNode {
public:
    int line = 0;
    virtual ~StmtNode() = default;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};

// ─── Expression nodes ────────────────────────────────────────────────────────

class NumNode : public ASTNode {
    double _val;
public:
    explicit NumNode(double v) : _val(v) {}
    double val() const { return _val; }
    void dump(std::string pfx, bool last) const override {
        std::cout << pfx << (last ? "└── " : "├── ") << "Num(" << _val << ")\n";
    }
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};

class StrNode : public ASTNode {
    std::string _val;
public:
    explicit StrNode(std::string v) : _val(std::move(v)) {}
    const std::string& val() const { return _val; }
    void dump(std::string pfx, bool last) const override {
        std::cout << pfx << (last ? "└── " : "├── ") << "Str(\"" << _val << "\")\n";
    }
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};

class NoneNode : public ASTNode {
public:
    void dump(std::string pfx, bool last) const override {
        std::cout << pfx << (last ? "└── " : "├── ") << "none\n";
    }
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};

class ConstNode : public ASTNode { // PI, E, INF, MAX
    Op _which;
public:
    explicit ConstNode(Op w) : _which(w) {}
    Op which() const { return _which; }
    void dump(std::string pfx, bool last) const override;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};

class VarNode : public ASTNode {
    bool    _local;
    int     _hops = 0;
    union { size_t gAddr; int32_t lOff; };
public:
    explicit VarNode(size_t addr)          : _local(false), _hops(0), gAddr(addr) {}
    VarNode(int32_t off, int hops = 0)     : _local(true),  _hops(hops), lOff(off)  {}
    bool    isLocal()   const { return _local; }
    int     hops()      const { return _hops;  }
    size_t  globalAddr()const { return gAddr;  }
    int32_t localOff()  const { return lOff;   }
    void dump(std::string pfx, bool last) const override;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};

class BinNode : public ASTNode {
    std::string _op;
    std::shared_ptr<ASTNode> _l, _r;
public:
    BinNode(std::string op, std::shared_ptr<ASTNode> l, std::shared_ptr<ASTNode> r)
        : _op(std::move(op)), _l(std::move(l)), _r(std::move(r)) {}
    const std::string& op()  const { return _op; }
    std::shared_ptr<ASTNode> left()  const { return _l; }
    std::shared_ptr<ASTNode> right() const { return _r; }
    Op opCode() const;
    void dump(std::string pfx, bool last) const override;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {_l, _r}; }
};

class UnaryNode : public ASTNode {
    std::string _op;
    std::shared_ptr<ASTNode> _child;
public:
    UnaryNode(std::string op, std::shared_ptr<ASTNode> c)
        : _op(std::move(op)), _child(std::move(c)) {}
    const std::string& op()    const { return _op;    }
    std::shared_ptr<ASTNode> child() const { return _child; }
    void dump(std::string pfx, bool last) const override;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {_child}; }
};

class TernaryNode : public ASTNode {
    std::shared_ptr<ASTNode> _cond, _then, _else;
public:
    TernaryNode(std::shared_ptr<ASTNode> c, std::shared_ptr<ASTNode> t, std::shared_ptr<ASTNode> e)
        : _cond(std::move(c)), _then(std::move(t)), _else(std::move(e)) {}
    std::shared_ptr<ASTNode> cond()     const { return _cond; }
    std::shared_ptr<ASTNode> thenExpr() const { return _then; }
    std::shared_ptr<ASTNode> elseExpr() const { return _else; }
    void dump(std::string pfx, bool last) const override;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {_cond, _then, _else}; }
};

class CallNode : public ASTNode {
    std::string _name;
    std::vector<std::shared_ptr<ASTNode>> _args;
public:
    CallNode(std::string n, std::vector<std::shared_ptr<ASTNode>> a)
        : _name(std::move(n)), _args(std::move(a)) {}
    const std::string& name()     const { return _name; }
    const std::vector<std::shared_ptr<ASTNode>>& args() const { return _args; }
    void dump(std::string pfx, bool last) const override;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};

class IndexNode : public ASTNode {
    std::shared_ptr<ASTNode> _str, _idx;
public:
    IndexNode(std::shared_ptr<ASTNode> s, std::shared_ptr<ASTNode> i)
        : _str(std::move(s)), _idx(std::move(i)) {}
    std::shared_ptr<ASTNode> str() const { return _str; }
    std::shared_ptr<ASTNode> idx() const { return _idx; }
    void dump(std::string pfx, bool last) const override {
        std::cout << pfx << (last ? "└── " : "├── ") << "Index[]\n";
        _str->dump(pfx + (last ? "    " : "│   "), false);
        _idx->dump(pfx + (last ? "    " : "│   "), true);
    }
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {_str, _idx}; }
};

class LenNode : public ASTNode {
    std::shared_ptr<ASTNode> _arg;
public:
    explicit LenNode(std::shared_ptr<ASTNode> a) : _arg(std::move(a)) {}
    std::shared_ptr<ASTNode> arg() const { return _arg; }
    void dump(std::string pfx, bool last) const override {
        std::cout << pfx << (last ? "└── " : "├── ") << "length\n";
        _arg->dump(pfx + (last ? "    " : "│   "), true);
    }
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {_arg}; }
};

// ─── Statement nodes ─────────────────────────────────────────────────────────

class Block : public StmtNode {
    std::vector<std::shared_ptr<StmtNode>> _stmts;
public:
    void add(std::shared_ptr<StmtNode> s) { _stmts.push_back(std::move(s)); }
    const std::vector<std::shared_ptr<StmtNode>>& stmts() const { return _stmts; }
    void dump(std::string pfx, bool last) const override;
};

class AssignStmt : public StmtNode {
    bool    _local;
    int32_t _lOff;
    int     _hops = 0;
    size_t  _gAddr;
    std::shared_ptr<ASTNode> _val;
public:
    AssignStmt(int32_t off, std::shared_ptr<ASTNode> v, int hops = 0)
        : _local(true), _lOff(off), _hops(hops), _gAddr(0), _val(std::move(v)) {}
    AssignStmt(size_t addr, std::shared_ptr<ASTNode> v)
        : _local(false), _lOff(0), _hops(0), _gAddr(addr), _val(std::move(v)) {}
    bool    isLocal()    const { return _local; }
    int32_t localOff()   const { return _lOff;  }
    int     hops()       const { return _hops;  }
    size_t  globalAddr() const { return _gAddr; }
    std::shared_ptr<ASTNode> value() const { return _val; }
    void dump(std::string pfx, bool last) const override;
};

class IfStmt : public StmtNode {
    std::shared_ptr<ASTNode>  _cond;
    std::shared_ptr<StmtNode> _then, _else;
public:
    IfStmt(std::shared_ptr<ASTNode> c, std::shared_ptr<StmtNode> t,
           std::shared_ptr<StmtNode> e = nullptr)
        : _cond(std::move(c)), _then(std::move(t)), _else(std::move(e)) {}
    std::shared_ptr<ASTNode>  cond()   const { return _cond; }
    std::shared_ptr<StmtNode> thenBr() const { return _then; }
    std::shared_ptr<StmtNode> elseBr() const { return _else; }
    void dump(std::string pfx, bool last) const override;
};

class WhileStmt : public StmtNode {
    std::shared_ptr<ASTNode>  _cond;
    std::shared_ptr<StmtNode> _body;
public:
    WhileStmt(std::shared_ptr<ASTNode> c, std::shared_ptr<StmtNode> b)
        : _cond(std::move(c)), _body(std::move(b)) {}
    std::shared_ptr<ASTNode>  cond() const { return _cond; }
    std::shared_ptr<StmtNode> body() const { return _body; }
    void dump(std::string pfx, bool last) const override;
};

class ForStmt : public StmtNode {
    std::shared_ptr<StmtNode> _init;
    std::shared_ptr<ASTNode>  _cond;
    std::shared_ptr<StmtNode> _update, _body;
public:
    ForStmt(std::shared_ptr<StmtNode> i, std::shared_ptr<ASTNode> c,
            std::shared_ptr<StmtNode> u, std::shared_ptr<StmtNode> b)
        : _init(std::move(i)), _cond(std::move(c)), _update(std::move(u)), _body(std::move(b)) {}
    std::shared_ptr<StmtNode> init()   const { return _init;   }
    std::shared_ptr<ASTNode>  cond()   const { return _cond;   }
    std::shared_ptr<StmtNode> update() const { return _update; }
    std::shared_ptr<StmtNode> body()   const { return _body;   }
    void dump(std::string pfx, bool last) const override;
};

class PrintStmt : public StmtNode {
    std::vector<std::shared_ptr<ASTNode>> _exprs;
public:
    explicit PrintStmt(std::vector<std::shared_ptr<ASTNode>> e) : _exprs(std::move(e)) {}
    const std::vector<std::shared_ptr<ASTNode>>& exprs() const { return _exprs; }
    void dump(std::string pfx, bool last) const override;
};

class FnDefStmt : public StmtNode {
    std::string _name;
    std::vector<std::string> _params;
    std::shared_ptr<StmtNode> _body;
    int  _slots;
    bool _isVoid;
public:
    FnDefStmt(std::string n, std::vector<std::string> p,
              std::shared_ptr<StmtNode> b, int s, bool v)
        : _name(std::move(n)), _params(std::move(p)), _body(std::move(b)),
          _slots(s), _isVoid(v) {}
    const std::string& name()   const { return _name;   }
    const std::vector<std::string>& params() const { return _params; }
    std::shared_ptr<StmtNode> body()   const { return _body;   }
    int  slots()  const { return _slots;  }
    bool isVoid() const { return _isVoid; }
    void dump(std::string pfx, bool last) const override;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};

class CallStmt : public StmtNode {
    std::shared_ptr<CallNode> _call;
public:
    explicit CallStmt(std::shared_ptr<ASTNode> c)
        : _call(std::dynamic_pointer_cast<CallNode>(c)) {}
    std::shared_ptr<CallNode> call() const { return _call; }
    void dump(std::string pfx, bool last) const override { _call->dump(pfx, last); }
};

class ReturnStmt : public StmtNode {
    std::shared_ptr<ASTNode> _expr;
public:
    explicit ReturnStmt(std::shared_ptr<ASTNode> e) : _expr(std::move(e)) {}
    std::shared_ptr<ASTNode> expr() const { return _expr; }
    void dump(std::string pfx, bool last) const override;
};

class BreakStmt : public StmtNode {
public:
    void dump(std::string pfx, bool last) const override {
        std::cout << pfx << (last ? "└── " : "├── ") << "break\n";
    }
};

class ContinueStmt : public StmtNode {
public:
    void dump(std::string pfx, bool last) const override {
        std::cout << pfx << (last ? "└── " : "├── ") << "continue\n";
    }
};

struct CaseClause {
    std::vector<std::shared_ptr<ASTNode>> vals;
    std::shared_ptr<StmtNode> body;
};

class SwitchStmt : public StmtNode {
    std::shared_ptr<ASTNode>   _expr;
    std::vector<CaseClause>    _cases;
    std::shared_ptr<StmtNode>  _default;
public:
    SwitchStmt(std::shared_ptr<ASTNode> e, std::vector<CaseClause> c,
               std::shared_ptr<StmtNode> d)
        : _expr(std::move(e)), _cases(std::move(c)), _default(std::move(d)) {}
    std::shared_ptr<ASTNode>          expr()    const { return _expr;    }
    const std::vector<CaseClause>&    cases()   const { return _cases;   }
    std::shared_ptr<StmtNode>         defBody() const { return _default; }
    void dump(std::string pfx, bool last) const override;
    std::vector<std::shared_ptr<ASTNode>> children() const override { return {}; }
};
