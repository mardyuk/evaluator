#include "parser.hpp"
#include <cmath>
#include <stdexcept>
#include <unordered_set>

static const std::unordered_set<std::string> kBuiltinNames = {
    "sin","cos","tan","asin","acos","atan","atan2",
    "sqrt","cbrt","pow","exp","log","ln","log10","log2","log_ab",
    "ceil","floor","abs","round","fmod",
    "input","length","type","chr","ord","bin","oct","hex","dec",
};

Parser::Parser(Tokenizer& tok, SymbolTable& st) : _tok(tok), _sym(st) {
    next(); // prime the lookahead
}

void Parser::error(const std::string& msg) const {
    throw std::runtime_error("Line " + std::to_string(_cur.line) + ": " + msg);
}

void Parser::expect(TokType t, const std::string& msg) {
    if (_cur.type != t) error(msg);
    next();
}

// ─── Program entry ───────────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseProgram() {
    _sym.beginProgram();
    auto prog = std::make_shared<Block>();
    while (!check(TokType::Eof)) {
        int ln = _cur.line;
        auto s = parseStatement();
        if (!s) continue;
        s->line = ln;
        checkTopLevel(s, ln);
        prog->add(s);
    }
    _sym.endProgram();
    if (!_hasMain)
        throw std::runtime_error("Program must define void fn main()");
    return prog;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

bool Parser::isTopLevel() const {
    return !_sym.insideFunction() && _sym.depth() <= 1;
}

bool Parser::defaultToLocal(bool explicitGlobal) const {
    if (explicitGlobal) return false;
    return _sym.insideFunction() || _sym.depth() > 1;
}

void Parser::checkTopLevel(const std::shared_ptr<StmtNode>& s, int ln) const {
    if (!s || !isTopLevel()) return;
    if (std::dynamic_pointer_cast<FnDefStmt>(s)) return;
    if (auto blk = std::dynamic_pointer_cast<Block>(s)) {
        for (auto& c : blk->stmts()) checkTopLevel(c, ln);
        return;
    }
    if (auto a = std::dynamic_pointer_cast<AssignStmt>(s)) {
        if (!a->isLocal()) return; // global decl is fine at top level
        throw std::runtime_error("Line " + std::to_string(ln)
            + ": local declarations are not allowed at top level");
    }
    throw std::runtime_error("Line " + std::to_string(ln)
        + ": executable statements must be inside fn main()");
}

// ─── Statement dispatch ──────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseStatement() {
    int ln = _cur.line;

    // fn / void fn
    if (check(TokType::Fn) || check(TokType::Void)) return parseFnDef();

    // let
    if (check(TokType::Let))    return parseLetDecl();

    // explicit global / local without let  (global x = ...; local x = ...;)
    if (check(TokType::Global) || check(TokType::Local)) {
        bool isGlobal = check(TokType::Global);
        next();
        if (!check(TokType::Name)) error("expected variable name after scope keyword");
        std::string name = _cur.text; next();
        return parseAssign(name, true, !isGlobal, isGlobal);
    }

    if (check(TokType::If))       return parseIf();
    if (check(TokType::While))    return parseWhile();
    if (check(TokType::For))      return parseFor();
    if (check(TokType::LBrace))   return parseBlock();
    if (check(TokType::Print))    return parsePrint();
    if (check(TokType::Return))   return parseReturn();
    if (check(TokType::Switch))   return parseSwitch();

    if (check(TokType::Break)) {
        if (!_inLoop && !_inSwitch)
            error("break outside loop or switch");
        next();
        expect(TokType::Semi, "expected ';' after break");
        auto s = std::make_shared<BreakStmt>(); s->line = ln; return s;
    }
    if (check(TokType::Continue)) {
        if (!_inLoop) error("continue outside loop");
        next();
        expect(TokType::Semi, "expected ';' after continue");
        auto s = std::make_shared<ContinueStmt>(); s->line = ln; return s;
    }

    // identifier: assignment or function call
    if (check(TokType::Name)) {
        std::string name = _cur.text; next();

        // function call as statement
        if (check(TokType::LParen)) {
            auto call = parseCallOrBuiltin(name);
            expect(TokType::Semi, "expected ';' after function call");
            auto s = std::make_shared<CallStmt>(call); s->line = ln; return s;
        }

        // assignment / compound-assign / implicit declaration
        return parseAssign(name, false, false, false);
    }

    error("unexpected token '" + _cur.text + "'");
    return nullptr;
}

// ─── Block ───────────────────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseBlock() {
    expect(TokType::LBrace, "expected '{'");
    _sym.enterBlock();
    auto blk = std::make_shared<Block>();
    while (!check(TokType::RBrace) && !check(TokType::Eof)) {
        int ln = _cur.line;
        auto s = parseStatement();
        if (!s) continue;
        s->line = ln;
        blk->add(s);
    }
    _sym.exitBlock();
    expect(TokType::RBrace, "expected '}'");
    return blk;
}

// ─── If ──────────────────────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseIf() {
    int ln = _cur.line;
    if (!_inFn) error("if is only allowed inside a function");
    next(); // 'if'
    expect(TokType::LParen, "expected '(' after 'if'");
    auto cond = parseExpr();
    expect(TokType::RParen, "expected ')' after condition");
    auto thenBr = parseBlock();

    std::shared_ptr<StmtNode> elseBr;
    if (check(TokType::Else)) {
        next();
        if (check(TokType::If)) elseBr = parseIf();
        else                    elseBr = parseBlock();
    }
    auto s = std::make_shared<IfStmt>(cond, thenBr, elseBr); s->line = ln;
    return s;
}

// ─── While ───────────────────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseWhile() {
    int ln = _cur.line;
    if (!_inFn) error("while is only allowed inside a function");
    next(); // 'while'
    expect(TokType::LParen, "expected '(' after 'while'");
    auto cond = parseExpr();
    expect(TokType::RParen, "expected ')' after condition");

    bool outerLoop = _inLoop; _inLoop = true;
    auto body = parseBlock();
    _inLoop = outerLoop;

    auto s = std::make_shared<WhileStmt>(cond, body); s->line = ln;
    return s;
}

// ─── For ─────────────────────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseFor() {
    int ln = _cur.line;
    if (!_inFn) error("for is only allowed inside a function");
    next(); // 'for'
    expect(TokType::LParen, "expected '(' after 'for'");

    _sym.enterBlock(); // scope for loop variable

    // init
    std::shared_ptr<StmtNode> init;
    if (!check(TokType::Semi)) {
        if (check(TokType::Let)) {
            next();
            bool forceLocal = false, forceGlobal = false;
            if (check(TokType::Local))  { forceLocal  = true; next(); }
            if (check(TokType::Global)) { forceGlobal = true; next(); }
            if (!check(TokType::Name)) error("expected variable name in for-init");
            std::string name = _cur.text; next();
            init = parseAssign(name, true, forceLocal, forceGlobal);
        } else if (check(TokType::Name)) {
            std::string name = _cur.text; next();
            init = parseAssign(name, false, false, false);
        } else {
            error("expected initializer in for loop");
        }
    } else {
        expect(TokType::Semi, "");
    }

    // condition
    std::shared_ptr<ASTNode> cond;
    if (!check(TokType::Semi)) cond = parseExpr();
    expect(TokType::Semi, "expected ';' in for loop");

    // update (no trailing ';')
    std::shared_ptr<StmtNode> update;
    if (!check(TokType::RParen)) {
        if (check(TokType::Name)) {
            std::string name = _cur.text; next();
            // parseAssign doesn't call expect(Semi) for compound assign in for-update
            // We need the update without a semicolon - handle manually
            if (check(TokType::CompAssign)) {
                std::string op = _cur.text; next();
                auto rhs = parseExpr();
                // desugar: x op= e  →  x = x op e
                int32_t lOff = 0; int hops = 0;
                std::shared_ptr<ASTNode> lhsNode;
                if (_sym.tryResolveLocal(name, lOff, hops)) {
                    lhsNode = std::make_shared<VarNode>(lOff, hops);
                } else {
                    size_t gAddr = _sym.globalAddr(name);
                    lhsNode = std::make_shared<VarNode>(gAddr);
                }
                std::string baseOp = op.substr(0, op.size() - 1);
                auto rhs2 = foldBin(baseOp, lhsNode, rhs);
                if (_sym.tryResolveLocal(name, lOff, hops)) {
                    auto a = std::make_shared<AssignStmt>(lOff, rhs2, hops);
                    a->line = ln; update = a;
                } else {
                    size_t gAddr = 0; _sym.tryGlobalAddr(name, gAddr);
                    auto a = std::make_shared<AssignStmt>(gAddr, rhs2);
                    a->line = ln; update = a;
                }
            } else if (check(TokType::Assign)) {
                next();
                auto rhs = parseExpr();
                int32_t lOff2 = 0; int hops2 = 0;
                if (_sym.tryResolveLocal(name, lOff2, hops2)) {
                    auto a = std::make_shared<AssignStmt>(lOff2, rhs, hops2);
                    a->line = ln; update = a;
                } else {
                    size_t gAddr = _sym.globalAddr(name);
                    auto a = std::make_shared<AssignStmt>(gAddr, rhs);
                    a->line = ln; update = a;
                }
            }
        }
    }
    expect(TokType::RParen, "expected ')' in for loop");

    bool outerLoop = _inLoop; _inLoop = true;
    auto body = parseBlock();
    _inLoop = outerLoop;
    _sym.exitBlock();

    auto s = std::make_shared<ForStmt>(init, cond, update, body); s->line = ln;
    return s;
}

// ─── Print ───────────────────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parsePrint() {
    int ln = _cur.line;
    next(); // 'print'
    expect(TokType::LParen, "expected '(' after 'print'");
    std::vector<std::shared_ptr<ASTNode>> exprs;
    if (!check(TokType::RParen)) {
        exprs.push_back(parseExpr());
        while (check(TokType::Comma)) { next(); exprs.push_back(parseExpr()); }
    }
    expect(TokType::RParen, "expected ')' after print arguments");
    expect(TokType::Semi,   "expected ';' after print(...)");
    // single-arg print appends '\n' automatically
    if (exprs.size() == 1) {
        exprs.push_back(std::make_shared<StrNode>("\n"));
    }
    auto s = std::make_shared<PrintStmt>(std::move(exprs)); s->line = ln;
    return s;
}

// ─── Return ──────────────────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseReturn() {
    int ln = _cur.line;
    if (!_inFn) error("return outside function");
    next(); // 'return'
    std::shared_ptr<ASTNode> expr;
    if (!check(TokType::Semi)) expr = parseExpr();
    expect(TokType::Semi, "expected ';' after return");
    auto s = std::make_shared<ReturnStmt>(expr); s->line = ln;
    return s;
}

// ─── Switch ──────────────────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseSwitch() {
    int ln = _cur.line;
    if (!_inFn) error("switch is only allowed inside a function");
    next(); // 'switch'
    expect(TokType::LParen, "expected '(' after 'switch'");
    auto expr = parseExpr();
    expect(TokType::RParen, "expected ')'");
    expect(TokType::LBrace, "expected '{'");

    std::vector<CaseClause> cases;
    std::shared_ptr<StmtNode> defBody;

    bool outerSwitch = _inSwitch; _inSwitch = true;
    while (!check(TokType::RBrace) && !check(TokType::Eof)) {
        if (check(TokType::Case)) {
            next();
            CaseClause clause;
            clause.vals.push_back(parseExpr());
            while (check(TokType::Comma)) { next(); clause.vals.push_back(parseExpr()); }
            expect(TokType::Colon, "expected ':' after case value");
            auto body = std::make_shared<Block>();
            while (!check(TokType::Case) && !check(TokType::Default)
                   && !check(TokType::RBrace) && !check(TokType::Eof)) {
                int bln = _cur.line;
                auto s = parseStatement();
                if (!s) continue;
                s->line = bln;
                body->add(s);
            }
            clause.body = body;
            cases.push_back(std::move(clause));
        } else if (check(TokType::Default)) {
            next();
            expect(TokType::Colon, "expected ':' after default");
            auto body = std::make_shared<Block>();
            while (!check(TokType::RBrace) && !check(TokType::Eof)) {
                int bln = _cur.line;
                auto s = parseStatement();
                if (!s) continue;
                s->line = bln;
                body->add(s);
            }
            defBody = body;
        } else {
            error("expected 'case' or 'default' inside switch");
        }
    }
    _inSwitch = outerSwitch;
    expect(TokType::RBrace, "expected '}'");
    auto s = std::make_shared<SwitchStmt>(expr, std::move(cases), defBody);
    s->line = ln; return s;
}

// ─── Function definition ─────────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseFnDef() {
    int ln = _cur.line;
    bool isVoid = false;
    if (check(TokType::Void)) { isVoid = true; next(); }
    if (!check(TokType::Fn)) error("expected 'fn'");
    next(); // 'fn'

    if (!check(TokType::Name)) error("expected function name");
    std::string name = _cur.text; next();

    if (name == "main") {
        if (!isVoid) error("main must be declared void fn main()");
        _hasMain = true;
    }

    expect(TokType::LParen, "expected '(' in function definition");
    std::vector<std::string> params;
    if (!check(TokType::RParen)) {
        if (!check(TokType::Name)) error("expected parameter name");
        params.push_back(_cur.text); next();
        while (check(TokType::Comma)) {
            next();
            if (!check(TokType::Name)) error("expected parameter name");
            params.push_back(_cur.text); next();
        }
    }
    expect(TokType::RParen, "expected ')' in function definition");

    _sym.enterFunction();
    for (int i = 0; i < (int)params.size(); ++i) {
        _sym.localOff(params[i]); // register params as locals
    }

    bool outerFn = _inFn; _inFn = true;
    auto body = parseBlock();
    _inFn = outerFn;

    int slots = _sym.frameSlotCount();
    _sym.exitFunction();

    auto s = std::make_shared<FnDefStmt>(name, params, body, slots, isVoid);
    s->line = ln; return s;
}

// ─── Variable declaration (let) ──────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseLetDecl() {
    next(); // 'let'
    bool forceLocal = false, forceGlobal = false;

    // modifiers: let local / let global
    if (check(TokType::Local))  { forceLocal  = true; next(); }
    if (check(TokType::Global)) { forceGlobal = true; next(); }

    if (!check(TokType::Name)) error("expected variable name after 'let'");
    std::string first = _cur.text; next();

    // multi-variable:  let a, b, c = 1, 2, 3
    if (check(TokType::Comma)) {
        // collect remaining names
        std::vector<std::string> names = {first};
        while (check(TokType::Comma)) {
            next();
            if (!check(TokType::Name)) error("expected variable name in let list");
            names.push_back(_cur.text); next();
        }
        expect(TokType::Assign, "expected '=' in multiple let declaration");
        std::vector<std::shared_ptr<ASTNode>> vals;
        vals.push_back(parseExpr());
        while (check(TokType::Comma)) { next(); vals.push_back(parseExpr()); }
        expect(TokType::Semi, "expected ';' after let declaration");

        auto blk = std::make_shared<Block>();
        for (size_t i = 0; i < names.size(); ++i) {
            auto val = (i < vals.size()) ? vals[i] : std::make_shared<NoneNode>();
            bool useLocal = forceLocal || defaultToLocal(forceGlobal);
            if (_sym.hasInInnermost(names[i]) || (useLocal && _sym.isLocal(names[i])))
                error("variable '" + names[i] + "' already declared in this scope");
            if (!useLocal && _sym.hasGlobal(names[i]))
                error("variable '" + names[i] + "' already declared in this scope");
            std::shared_ptr<AssignStmt> a;
            if (useLocal) {
                int32_t off = _sym.localOff(names[i]);
                a = std::make_shared<AssignStmt>(off, val);
            } else {
                size_t addr = _sym.globalAddr(names[i]);
                a = std::make_shared<AssignStmt>(addr, val);
            }
            blk->add(a);
        }
        return blk;
    }

    return parseAssign(first, true, forceLocal, forceGlobal);
}

// ─── Assignment / declaration ────────────────────────────────────────────────

std::shared_ptr<StmtNode> Parser::parseAssign(const std::string& name,
                                                bool freshDecl,
                                                bool forceLocal,
                                                bool forceGlobal) {
    int ln = _cur.line;
    bool useLocal = forceLocal || defaultToLocal(forceGlobal);

    if (freshDecl) {
        if (useLocal && _sym.hasInInnermost(name))
            error("variable '" + name + "' already declared in this scope");
        if (!useLocal && _sym.hasGlobal(name))
            error("variable '" + name + "' already declared in this scope");
    }

    // Declaration only: let x;
    if (check(TokType::Semi)) {
        next();
        if (useLocal) {
            int32_t off = _sym.localOff(name);
            auto a = std::make_shared<AssignStmt>(off, std::make_shared<NoneNode>());
            a->line = ln; return a;
        } else {
            size_t addr = _sym.globalAddr(name);
            auto a = std::make_shared<AssignStmt>(addr, std::make_shared<NoneNode>());
            a->line = ln; return a;
        }
    }

    // Compound assignment:  x op= expr;
    if (check(TokType::CompAssign)) {
        std::string op = _cur.text; next();
        auto rhs = parseExpr();
        expect(TokType::Semi, "expected ';' after assignment");

        // Build: x = x op rhs
        int32_t lOff = 0; int hops = 0;
        std::shared_ptr<ASTNode> lhsNode;
        if (_sym.tryResolveLocal(name, lOff, hops)) {
            lhsNode = std::make_shared<VarNode>(lOff, hops);
        } else {
            size_t gAddr = 0;
            if (!_sym.tryGlobalAddr(name, gAddr)) {
                if (!freshDecl) error("undefined variable '" + name + "'");
                gAddr = _sym.globalAddr(name);
            }
            lhsNode = std::make_shared<VarNode>(gAddr);
        }
        std::string baseOp = op.substr(0, op.size() - 1);
        auto combined = foldBin(baseOp, lhsNode, rhs);

        if (_sym.tryResolveLocal(name, lOff, hops)) {
            auto a = std::make_shared<AssignStmt>(lOff, combined, hops);
            a->line = ln; return a;
        } else {
            size_t gAddr = 0; _sym.tryGlobalAddr(name, gAddr);
            auto a = std::make_shared<AssignStmt>(gAddr, combined);
            a->line = ln; return a;
        }
    }

    // Regular assignment: x = expr;
    expect(TokType::Assign, "expected '=' or ';' after variable name");
    auto rhs = parseExpr();
    expect(TokType::Semi, "expected ';' after assignment");

    if (useLocal) {
        int32_t off = _sym.localOff(name);
        auto a = std::make_shared<AssignStmt>(off, rhs);
        a->line = ln; return a;
    } else {
        size_t addr = _sym.globalAddr(name);
        auto a = std::make_shared<AssignStmt>(addr, rhs);
        a->line = ln; return a;
    }
}

// ─── Expression (shunting-yard) ──────────────────────────────────────────────

int Parser::prec(const std::string& op) const {
    if (op == "or")                              return 1;
    if (op == "and")                             return 2;
    if (op == "==" || op == "!=")               return 3;
    if (op == "<" || op == ">" ||
        op == "<=" || op == ">=")               return 4;
    if (op == "|")                               return 5;
    if (op == "^")                               return 6;
    if (op == "&")                               return 7;
    if (op == "<<" || op == ">>")               return 8;
    if (op == "+" || op == "-")                 return 9;
    if (op == "*" || op == "/" ||
        op == "%" || op == "//" || op == "%/")  return 10;
    if (op == "**")                              return 11; // right-assoc
    return 0;
}

bool Parser::isRightAssoc(const std::string& op) const {
    return op == "**";
}

void Parser::applyOp(const std::string& op, std::stack<std::shared_ptr<ASTNode>>& vals) {
    if (op == "not" || op == "~" || op == "_") { // unary
        if (vals.empty()) error("missing operand for unary '" + op + "'");
        auto child = vals.top(); vals.pop();
        vals.push(foldUnary(op, child));
        return;
    }
    if (vals.size() < 2) error("missing operand for operator '" + op + "'");
    auto r = vals.top(); vals.pop();
    auto l = vals.top(); vals.pop();
    vals.push(foldBin(op, l, r));
}

void Parser::pushOp(const std::string& op,
                    std::stack<std::string>& ops,
                    std::stack<std::shared_ptr<ASTNode>>& vals) {
    while (!ops.empty() && ops.top() != "(" &&
           (prec(ops.top()) > prec(op) ||
            (prec(ops.top()) == prec(op) && !isRightAssoc(op)))) {
        applyOp(ops.top(), vals);
        ops.pop();
    }
    ops.push(op);
}

std::shared_ptr<ASTNode> Parser::parseExpr() {
    std::stack<std::string>               ops;
    std::stack<std::shared_ptr<ASTNode>>  vals;
    bool expectOperand = true;

    while (true) {
        TokType t = _cur.type;

        // ── Operands ──────────────────────────────────────────────────────────
        if (expectOperand) {
            if (t == TokType::Num) {
                std::string s = _cur.text; next();
                double v = 0;
                try {
                    if (s.size() > 2 && s[0] == '0' &&
                        (s[1] == 'x' || s[1] == 'X'))     v = std::stod(std::string("0x") + s.substr(2)); // still double
                    else                                    v = std::stod(s);
                } catch (...) { error("invalid number: " + s); }
                // Handle different bases
                if (s.size() > 2 && s[0] == '0') {
                    if (s[1] == 'x' || s[1] == 'X') v = (double)std::stoull(s.substr(2), nullptr, 16);
                    else if (s[1] == 'b' || s[1] == 'B') v = (double)std::stoull(s.substr(2), nullptr, 2);
                    else if (s[1] == 'o' || s[1] == 'O') v = (double)std::stoull(s.substr(2), nullptr, 8);
                }
                vals.push(std::make_shared<NumNode>(v));
                expectOperand = false;

            } else if (t == TokType::Str) {
                vals.push(std::make_shared<StrNode>(_cur.text)); next();
                expectOperand = false;

            } else if (t == TokType::Bool) {
                vals.push(std::make_shared<NumNode>(_cur.text == "true" ? 1.0 : 0.0)); next();
                expectOperand = false;

            } else if (t == TokType::None) {
                vals.push(std::make_shared<NoneNode>()); next();
                expectOperand = false;

            } else if (t == TokType::MathConst) {
                Op which = Op::CONST_PI;
                if (_cur.text == "E")   which = Op::CONST_E;
                if (_cur.text == "INF") which = Op::CONST_INF;
                if (_cur.text == "MAX") which = Op::CONST_MAX;
                vals.push(std::make_shared<ConstNode>(which)); next();
                expectOperand = false;

            } else if (t == TokType::Name) {
                std::string name = _cur.text; next();
                if (check(TokType::LParen)) {
                    vals.push(parseCallOrBuiltin(name));
                } else {
                    // variable reference
                    int32_t lOff = 0; int hops = 0;
                    if (_sym.tryResolveLocal(name, lOff, hops)) {
                        vals.push(std::make_shared<VarNode>(lOff, hops));
                    } else {
                        size_t gAddr = 0;
                        if (_sym.tryGlobalAddr(name, gAddr)) {
                            vals.push(std::make_shared<VarNode>(gAddr));
                        } else {
                            // implicit global declaration
                            gAddr = _sym.globalAddr(name);
                            vals.push(std::make_shared<VarNode>(gAddr));
                        }
                    }
                }
                expectOperand = false;

            } else if (t == TokType::LParen) {
                next(); ops.push("("); // sub-expression

            } else if (t == TokType::Op && _cur.text == "-") {
                next(); ops.push("_"); // unary minus marker

            } else if (t == TokType::Op && _cur.text == "+") {
                next(); // unary plus: no-op

            } else if (t == TokType::Not) {
                next(); ops.push("not");

            } else if (t == TokType::Op && _cur.text == "~") {
                next(); ops.push("~");

            } else {
                error("expected an expression, got '" + _cur.text + "'");
            }

        // ── Operators ─────────────────────────────────────────────────────────
        } else {
            // ternary
            if (t == TokType::Question) {
                next();
                // flush remaining ops first
                while (!ops.empty() && ops.top() != "(") {
                    applyOp(ops.top(), vals); ops.pop();
                }
                auto cond = vals.top(); vals.pop();
                auto thenE = parseExpr();
                expect(TokType::Colon, "expected ':' in ternary expression");
                auto elseE = parseExpr();
                vals.push(std::make_shared<TernaryNode>(cond, thenE, elseE));
                break;
            }

            if (t == TokType::RParen) {
                // pop until matching '('
                while (!ops.empty() && ops.top() != "(") {
                    applyOp(ops.top(), vals); ops.pop();
                }
                if (ops.empty()) break; // closing paren belongs to caller
                ops.pop(); // pop '('
                next();

            } else if ((t == TokType::Op || t == TokType::Compare
                        || t == TokType::And || t == TokType::Or) ) {
                std::string op = _cur.text;
                if (t == TokType::And) op = "and";
                if (t == TokType::Or)  op = "or";
                next();
                pushOp(op, ops, vals);
                expectOperand = true;

            } else {
                break; // not an operator → end of expression
            }
        }
    }

    // drain remaining operators
    while (!ops.empty()) {
        if (ops.top() == "(") error("unmatched '(' in expression");
        applyOp(ops.top(), vals); ops.pop();
    }
    if (vals.empty()) error("empty expression");
    return vals.top();
}

// ─── Function / built-in call ────────────────────────────────────────────────

std::shared_ptr<ASTNode> Parser::parseCallOrBuiltin(const std::string& name) {
    expect(TokType::LParen, "expected '(' in function call");
    std::vector<std::shared_ptr<ASTNode>> args;
    if (!check(TokType::RParen)) {
        args.push_back(parseExpr());
        while (check(TokType::Comma)) { next(); args.push_back(parseExpr()); }
    }
    expect(TokType::RParen, "expected ')' after arguments");

    if (name == "length") {
        if (args.size() != 1) error("length() expects exactly 1 argument");
        return std::make_shared<LenNode>(args[0]);
    }
    return std::make_shared<CallNode>(name, std::move(args));
}

// ─── Constant folding ────────────────────────────────────────────────────────

std::shared_ptr<ASTNode> Parser::foldBin(const std::string& op,
                                          std::shared_ptr<ASTNode> l,
                                          std::shared_ptr<ASTNode> r) {
    auto ln = std::dynamic_pointer_cast<NumNode>(l);
    auto rn = std::dynamic_pointer_cast<NumNode>(r);
    if (!ln || !rn) return std::make_shared<BinNode>(op, l, r);

    double a = ln->val(), b = rn->val();
    auto ll = (long long)a, rl = (long long)b;

    if (op == "+")   return std::make_shared<NumNode>(a + b);
    if (op == "-")   return std::make_shared<NumNode>(a - b);
    if (op == "*")   return std::make_shared<NumNode>(a * b);
    if (op == "**")  return std::make_shared<NumNode>(std::pow(a, b));
    if (op == "/")   { if (b == 0) return std::make_shared<BinNode>(op, l, r);
                       return std::make_shared<NumNode>(a / b); }
    if (op == "//")  { if (b == 0) return std::make_shared<BinNode>(op, l, r);
                       return std::make_shared<NumNode>(std::floor(a / b)); }
    if (op == "%/")  { if (b == 0) return std::make_shared<BinNode>(op, l, r);
                       return std::make_shared<NumNode>(a / b - std::floor(a / b)); }
    if (op == "%")   { if (rl == 0) return std::make_shared<BinNode>(op, l, r);
                       return std::make_shared<NumNode>((double)(ll % rl)); }
    if (op == "&")   return std::make_shared<NumNode>((double)(ll & rl));
    if (op == "|")   return std::make_shared<NumNode>((double)(ll | rl));
    if (op == "^")   return std::make_shared<NumNode>((double)(ll ^ rl));
    if (op == "<<")  return std::make_shared<NumNode>((double)(ll << (rl & 0x1F)));
    if (op == ">>")  return std::make_shared<NumNode>((double)((uint32_t)ll >> (rl & 0x1F)));
    if (op == "==")  return std::make_shared<NumNode>(a == b ? 1.0 : 0.0);
    if (op == "!=")  return std::make_shared<NumNode>(a != b ? 1.0 : 0.0);
    if (op == "<")   return std::make_shared<NumNode>(a < b  ? 1.0 : 0.0);
    if (op == ">")   return std::make_shared<NumNode>(a > b  ? 1.0 : 0.0);
    if (op == "<=")  return std::make_shared<NumNode>(a <= b ? 1.0 : 0.0);
    if (op == ">=")  return std::make_shared<NumNode>(a >= b ? 1.0 : 0.0);
    if (op == "and") return std::make_shared<NumNode>((a != 0 && b != 0) ? 1.0 : 0.0);
    if (op == "or")  return std::make_shared<NumNode>((a != 0 || b != 0) ? 1.0 : 0.0);
    return std::make_shared<BinNode>(op, l, r);
}

std::shared_ptr<ASTNode> Parser::foldUnary(const std::string& op,
                                            std::shared_ptr<ASTNode> child) {
    auto n = std::dynamic_pointer_cast<NumNode>(child);
    if (!n) return std::make_shared<UnaryNode>(op, child);
    if (op == "_") return std::make_shared<NumNode>(-n->val());
    if (op == "~") return std::make_shared<NumNode>((double)(~(long long)n->val()));
    if (op == "not") return std::make_shared<NumNode>(n->val() == 0 ? 1.0 : 0.0);
    return std::make_shared<UnaryNode>(op, child);
}
