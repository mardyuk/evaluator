#include "compiler.hpp"
#include <cmath>
#include <fstream>
#include <cstring>
#include <limits>
#include <stdexcept>

// ─── Register allocation ─────────────────────────────────────────────────────

int Compiler::allocReg() {
    if (!_freeRegs.empty()) {
        int r = _freeRegs.top(); _freeRegs.pop(); return r;
    }
    while (_nextReg == SP || _nextReg == FP) ++_nextReg;
    return _nextReg++;
}

void Compiler::freeReg(int r) {
    if (r != SP && r != FP) _freeRegs.push(r);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

int Compiler::constIdx(double v) {
    auto it = _constMap.find(v);
    if (it != _constMap.end()) return it->second;
    int idx = (int)_constPool.size();
    _constPool.push_back(v);
    _constMap[v] = idx;
    return idx;
}

int Compiler::strIdx(const std::string& s) {
    auto it = _strMap.find(s);
    if (it != _strMap.end()) return it->second;
    int idx = (int)_strPool.size();
    _strPool.push_back(s);
    _strMap[s] = idx;
    return idx;
}

void Compiler::addLines(int line, size_t count) {
    _lines.insert(_lines.end(), count, line);
}

void Compiler::rebaseJumps(std::vector<Instr>& chunk, uint16_t base) {
    for (auto& inst : chunk)
        if (inst.op == (uint32_t)Op::JZ || inst.op == (uint32_t)Op::JMP)
            setAddr(inst, (uint16_t)(getAddr(inst) + base));
}

void Compiler::emitPrologue(std::vector<Instr>& code, int slots) {
    if (slots < 1) slots = 1;
    int frameSize = (slots + 4) * 4;
    code.push_back({(uint32_t)Op::ADDI, (uint32_t)SP, (uint32_t)SP, (uint32_t)(int32_t)(-frameSize)});
    _lines.push_back(0);
    code.push_back({(uint32_t)Op::ADDI, (uint32_t)FP, (uint32_t)SP, (uint32_t)frameSize});
    _lines.push_back(0);
}

// ─── Post-order traversal ────────────────────────────────────────────────────

std::vector<std::shared_ptr<ASTNode>> Compiler::postOrder(std::shared_ptr<ASTNode> root) {
    if (!root) return {};
    std::vector<std::shared_ptr<ASTNode>> result;
    std::stack<std::shared_ptr<ASTNode>> s1, s2;
    s1.push(root);
    while (!s1.empty()) {
        auto n = s1.top(); s1.pop(); s2.push(n);
        if (std::dynamic_pointer_cast<TernaryNode>(n)) continue; // handled inline
        for (auto& c : n->children()) s1.push(c);
    }
    while (!s2.empty()) { result.push_back(s2.top()); s2.pop(); }
    return result;
}

// ─── Optimizer ───────────────────────────────────────────────────────────────

std::shared_ptr<ASTNode> Compiler::optimize(std::shared_ptr<ASTNode> node) {
    if (!node) return nullptr;

    if (auto b = std::dynamic_pointer_cast<BinNode>(node)) {
        auto l = optimize(b->left()), r = optimize(b->right());
        auto ln = std::dynamic_pointer_cast<NumNode>(l);
        auto rn = std::dynamic_pointer_cast<NumNode>(r);
        if (ln && rn) {
            double a = ln->val(), bv = rn->val();
            auto ll = (long long)a, rl = (long long)bv;
            Op op = b->opCode();
            switch (op) {
                case Op::ADD:  return std::make_shared<NumNode>(a + bv);
                case Op::SUB:  return std::make_shared<NumNode>(a - bv);
                case Op::MUL:  return std::make_shared<NumNode>(a * bv);
                case Op::POW:  return std::make_shared<NumNode>(std::pow(a, bv));
                case Op::DIV:  if (bv != 0) return std::make_shared<NumNode>(a / bv); break;
                case Op::FDIV: if (bv != 0) return std::make_shared<NumNode>(std::floor(a/bv)); break;
                case Op::MOD:  if (rl  != 0) return std::make_shared<NumNode>((double)(ll%rl)); break;
                case Op::BAND: return std::make_shared<NumNode>((double)(ll & rl));
                case Op::BOR:  return std::make_shared<NumNode>((double)(ll | rl));
                case Op::BXOR: return std::make_shared<NumNode>((double)(ll ^ rl));
                case Op::SHL:  return std::make_shared<NumNode>((double)(ll << (rl & 0x1F)));
                case Op::SHR:  return std::make_shared<NumNode>((double)((uint32_t)ll >> (rl&0x1F)));
                case Op::EQ:   return std::make_shared<NumNode>(a == bv ? 1.0 : 0.0);
                case Op::NEQ:  return std::make_shared<NumNode>(a != bv ? 1.0 : 0.0);
                case Op::LT:   return std::make_shared<NumNode>(a <  bv ? 1.0 : 0.0);
                case Op::GT:   return std::make_shared<NumNode>(a >  bv ? 1.0 : 0.0);
                case Op::LEQ:  return std::make_shared<NumNode>(a <= bv ? 1.0 : 0.0);
                case Op::GEQ:  return std::make_shared<NumNode>(a >= bv ? 1.0 : 0.0);
                case Op::AND:  return std::make_shared<NumNode>((a&&bv)?1.0:0.0);
                case Op::OR:   return std::make_shared<NumNode>((a||bv)?1.0:0.0);
                default: break;
            }
        }
        return std::make_shared<BinNode>(b->op(), l, r);
    }
    if (auto u = std::dynamic_pointer_cast<UnaryNode>(node)) {
        auto child = optimize(u->child());
        auto n = std::dynamic_pointer_cast<NumNode>(child);
        if (n) {
            if (u->op() == "_") return std::make_shared<NumNode>(-n->val());
            if (u->op() == "~") return std::make_shared<NumNode>((double)(~(long long)n->val()));
            if (u->op() == "not") return std::make_shared<NumNode>(n->val()==0?1.0:0.0);
        }
        return std::make_shared<UnaryNode>(u->op(), child);
    }
    if (auto blk = std::dynamic_pointer_cast<Block>(node)) {
        auto nb = std::make_shared<Block>(); nb->line = blk->line;
        for (auto& s : blk->stmts()) {
            if (auto os = std::dynamic_pointer_cast<StmtNode>(optimize(s))) nb->add(os);
        }
        return nb;
    }
    if (auto a = std::dynamic_pointer_cast<AssignStmt>(node)) {
        std::shared_ptr<AssignStmt> na;
        if (a->isLocal()) na = std::make_shared<AssignStmt>(a->localOff(), optimize(a->value()), a->hops());
        else              na = std::make_shared<AssignStmt>(a->globalAddr(), optimize(a->value()));
        na->line = a->line; return na;
    }
    if (auto is = std::dynamic_pointer_cast<IfStmt>(node)) {
        auto cond = optimize(is->cond());
        auto thenB = std::dynamic_pointer_cast<StmtNode>(optimize(is->thenBr()));
        auto elseB = std::dynamic_pointer_cast<StmtNode>(optimize(is->elseBr()));
        if (auto cn = std::dynamic_pointer_cast<NumNode>(cond))
            return cn->val() != 0 ? thenB : (elseB ? elseB : nullptr);
        auto ns = std::make_shared<IfStmt>(cond, thenB, elseB); ns->line = is->line; return ns;
    }
    if (auto ws = std::dynamic_pointer_cast<WhileStmt>(node)) {
        auto cond = optimize(ws->cond());
        auto body = std::dynamic_pointer_cast<StmtNode>(optimize(ws->body()));
        if (auto cn = std::dynamic_pointer_cast<NumNode>(cond))
            if (cn->val() == 0) return nullptr;
        auto ns = std::make_shared<WhileStmt>(cond, body); ns->line = ws->line; return ns;
    }
    if (auto ps = std::dynamic_pointer_cast<PrintStmt>(node)) {
        std::vector<std::shared_ptr<ASTNode>> exprs;
        for (auto& e : ps->exprs()) exprs.push_back(optimize(e));
        auto ns = std::make_shared<PrintStmt>(std::move(exprs)); ns->line = ps->line; return ns;
    }
    if (auto fs = std::dynamic_pointer_cast<ForStmt>(node)) {
        auto i = std::dynamic_pointer_cast<StmtNode>(optimize(fs->init()));
        auto c = optimize(fs->cond());
        auto u = std::dynamic_pointer_cast<StmtNode>(optimize(fs->update()));
        auto b = std::dynamic_pointer_cast<StmtNode>(optimize(fs->body()));
        auto ns = std::make_shared<ForStmt>(i, c, u, b); ns->line = fs->line; return ns;
    }
    return node;
}

// ─── Expression code generation ──────────────────────────────────────────────

bool Compiler::tryBuiltin(const std::string& name,
                           const std::vector<std::shared_ptr<ASTNode>>& args,
                           std::vector<Instr>& code, int& resultReg) {
    auto emitArg = [&](const std::shared_ptr<ASTNode>& arg) -> int {
        auto chunk = genExpr(postOrder(arg));
        rebaseJumps(chunk, (uint16_t)code.size());
        code.insert(code.end(), chunk.begin(), chunk.end());
        return chunk.empty() ? 0 : (int)chunk.back().dst;
    };
    auto unary = [&](Op op) -> bool {
        if (args.size() != 1) return false;
        int a = emitArg(args[0]);
        resultReg = allocReg();
        code.push_back({(uint32_t)op, (uint32_t)resultReg, (uint32_t)a, 0});
        return true;
    };
    auto binary = [&](Op op) -> bool {
        if (args.size() != 2) return false;
        int a = emitArg(args[0]), b = emitArg(args[1]);
        resultReg = allocReg();
        code.push_back({(uint32_t)op, (uint32_t)resultReg, (uint32_t)a, (uint32_t)b});
        return true;
    };

    if (name == "sin")    return unary(Op::SIN);
    if (name == "cos")    return unary(Op::COS);
    if (name == "tan")    return unary(Op::TAN);
    if (name == "asin")   return unary(Op::ASIN);
    if (name == "acos")   return unary(Op::ACOS);
    if (name == "atan")   return unary(Op::ATAN);
    if (name == "atan2")  return binary(Op::ATAN2);
    if (name == "sqrt")   return unary(Op::SQRT);
    if (name == "cbrt")   return unary(Op::CBRT);
    if (name == "pow")    return binary(Op::MATH_POW);
    if (name == "exp")    return unary(Op::EXP);
    if (name == "log")    return unary(Op::LOG);
    if (name == "ln")     return unary(Op::LOG);
    if (name == "log10")  return unary(Op::LOG10);
    if (name == "log2")   return unary(Op::LOG2);
    if (name == "log_ab") return binary(Op::LOG_AB);
    if (name == "ceil")   return unary(Op::CEIL);
    if (name == "floor")  return unary(Op::FLOOR);
    if (name == "abs")    return unary(Op::ABS);
    if (name == "round")  return unary(Op::ROUND);
    if (name == "fmod")   return binary(Op::FMOD);
    if (name == "type")   return unary(Op::TYPE);
    if (name == "ord")    return unary(Op::ORD);
    if (name == "chr")    return unary(Op::CHR);
    if (name == "bin")    return unary(Op::BIN);
    if (name == "oct")    return unary(Op::OCT);
    if (name == "hex")    return unary(Op::HEX);
    if (name == "dec")    return unary(Op::DEC);
    if (name == "random") {
        if (!args.empty()) return false;
        resultReg = allocReg();
        code.push_back({(uint32_t)Op::RANDOM, (uint32_t)resultReg, 0, 0});
        return true;
    }

    if (name == "input") {
        if (args.size() > 1) return false;
        resultReg = allocReg();
        if (!args.empty()) {
            int a = emitArg(args[0]);
            code.push_back({(uint32_t)Op::PRINT, (uint32_t)a, 0, 0});
        }
        code.push_back({(uint32_t)Op::INPUT, (uint32_t)resultReg, 0, 0});
        return true;
    }
    return false;
}

std::vector<Instr> Compiler::genExpr(const std::vector<std::shared_ptr<ASTNode>>& nodes) {
    std::vector<Instr> code;
    std::stack<int> stk;

    for (const auto& node : nodes) {
        if (auto n = std::dynamic_pointer_cast<NumNode>(node)) {
            int r = allocReg();
            int idx = constIdx(n->val());
            code.push_back({(uint32_t)Op::LOAD_CONST, (uint32_t)r, (uint32_t)idx, 0});
            stk.push(r);

        } else if (auto s = std::dynamic_pointer_cast<StrNode>(node)) {
            int r = allocReg();
            int idx = strIdx(s->val());
            code.push_back({(uint32_t)Op::LOAD_STR, (uint32_t)r, (uint32_t)idx, 0});
            stk.push(r);

        } else if (std::dynamic_pointer_cast<NoneNode>(node)) {
            int r = allocReg();
            code.push_back({(uint32_t)Op::LOAD_NONE, (uint32_t)r, 0, 0});
            stk.push(r);

        } else if (auto c = std::dynamic_pointer_cast<ConstNode>(node)) {
            int r = allocReg();
            code.push_back({(uint32_t)c->which(), (uint32_t)r, 0, 0});
            stk.push(r);

        } else if (auto v = std::dynamic_pointer_cast<VarNode>(node)) {
            int r = allocReg();
            if (v->isLocal()) {
                if (v->hops() > 0) {
                    code.push_back({(uint32_t)Op::LOAD_OUTER, (uint32_t)r,
                                    (uint32_t)v->hops(), (uint32_t)(uint8_t)(int8_t)v->localOff()});
                } else {
                    code.push_back({(uint32_t)Op::LOAD, (uint32_t)r,
                                    (uint32_t)FP, (uint32_t)v->localOff()});
                }
            } else {
                code.push_back({(uint32_t)Op::LOAD_VAR, (uint32_t)r, (uint32_t)v->globalAddr(), 0});
            }
            stk.push(r);

        } else if (auto b = std::dynamic_pointer_cast<BinNode>(node)) {
            int rr = stk.top(); stk.pop();
            int lr = stk.top(); stk.pop();
            int dst = allocReg();
            code.push_back({(uint32_t)b->opCode(), (uint32_t)dst, (uint32_t)lr, (uint32_t)rr});
            freeReg(lr); freeReg(rr);
            stk.push(dst);

        } else if (auto u = std::dynamic_pointer_cast<UnaryNode>(node)) {
            int cr = stk.top(); stk.pop();
            int dst = allocReg();
            Op uop = Op::NEG;
            if (u->op() == "not") uop = Op::LNOT;
            else if (u->op() == "~") uop = Op::BNOT;
            code.push_back({(uint32_t)uop, (uint32_t)dst, (uint32_t)cr, 0});
            freeReg(cr);
            stk.push(dst);

        } else if (auto ix = std::dynamic_pointer_cast<IndexNode>(node)) {
            // IndexNode children are visited by postOrder, but we handle it inline
            // (postOrder skips TernaryNode; IndexNode children ARE in the traversal)
            int idxR = stk.top(); stk.pop();
            int strR = stk.top(); stk.pop();
            int dst = allocReg();
            code.push_back({(uint32_t)Op::STR_GET, (uint32_t)dst, (uint32_t)strR, (uint32_t)idxR});
            freeReg(strR); freeReg(idxR);
            stk.push(dst);

        } else if (auto ln = std::dynamic_pointer_cast<LenNode>(node)) {
            int ar = stk.top(); stk.pop();
            int dst = allocReg();
            code.push_back({(uint32_t)Op::LENGTH, (uint32_t)dst, (uint32_t)ar, 0});
            freeReg(ar);
            stk.push(dst);

        } else if (auto call = std::dynamic_pointer_cast<CallNode>(node)) {
            int builtinReg = 0;
            if (tryBuiltin(call->name(), call->args(), code, builtinReg)) {
                stk.push(builtinReg);
                continue;
            }
            for (const auto& arg : call->args()) {
                auto chunk = genExpr(postOrder(arg));
                rebaseJumps(chunk, (uint16_t)code.size());
                code.insert(code.end(), chunk.begin(), chunk.end());
                int ar = chunk.empty() ? 0 : (int)chunk.back().dst;
                code.push_back({(uint32_t)Op::PUSH_ARG, (uint32_t)ar, 0, 0});
                freeReg(ar);
            }
            int dst = allocReg();
            Instr ci{}; ci.op = (uint32_t)Op::CALL; ci.dst = (uint32_t)dst;
            if (_fns.count(call->name())) {
                setAddr(ci, (uint16_t)_fns[call->name()].addr);
            } else {
                setAddr(ci, 0);
                _fwdCalls.push_back({code.size(), call->name()});
            }
            code.push_back(ci);
            stk.push(dst);

        } else if (auto tern = std::dynamic_pointer_cast<TernaryNode>(node)) {
            int dst = allocReg();
            auto condChunk = genExpr(postOrder(tern->cond()));
            rebaseJumps(condChunk, (uint16_t)code.size());
            code.insert(code.end(), condChunk.begin(), condChunk.end());
            int condR = condChunk.empty() ? 0 : (int)condChunk.back().dst;

            size_t jzIdx = code.size();
            code.push_back({(uint32_t)Op::JZ, (uint32_t)condR, 0, 0});

            auto thenChunk = genExpr(postOrder(tern->thenExpr()));
            rebaseJumps(thenChunk, (uint16_t)code.size());
            code.insert(code.end(), thenChunk.begin(), thenChunk.end());
            int thenR = thenChunk.empty() ? 0 : (int)thenChunk.back().dst;
            code.push_back({(uint32_t)Op::MOV, (uint32_t)dst, (uint32_t)thenR, 0});
            freeReg(thenR);

            size_t jmpIdx = code.size();
            code.push_back({(uint32_t)Op::JMP, 0, 0, 0});
            setAddr(code[jzIdx], (uint16_t)code.size());

            auto elseChunk = genExpr(postOrder(tern->elseExpr()));
            rebaseJumps(elseChunk, (uint16_t)code.size());
            code.insert(code.end(), elseChunk.begin(), elseChunk.end());
            int elseR = elseChunk.empty() ? 0 : (int)elseChunk.back().dst;
            code.push_back({(uint32_t)Op::MOV, (uint32_t)dst, (uint32_t)elseR, 0});
            freeReg(elseR);

            setAddr(code[jmpIdx], (uint16_t)code.size());
            freeReg(condR);
            stk.push(dst);
        }
    }
    return code;
}

// ─── Statement compilation ───────────────────────────────────────────────────

void Compiler::compileStmt(std::shared_ptr<StmtNode> stmt, std::vector<Instr>& code) {
    if (!stmt) return;

    if (auto a = std::dynamic_pointer_cast<AssignStmt>(stmt)) {
        auto chunk = genExpr(postOrder(a->value()));
        rebaseJumps(chunk, (uint16_t)code.size());
        code.insert(code.end(), chunk.begin(), chunk.end());
        addLines(a->line, chunk.size());
        int src = chunk.empty() ? 0 : (int)chunk.back().dst;
        if (a->isLocal()) {
            if (a->hops() > 0) {
                code.push_back({(uint32_t)Op::STORE_OUTER, (uint32_t)src,
                                (uint32_t)a->hops(), (uint32_t)(uint8_t)(int8_t)a->localOff()});
            } else {
                code.push_back({(uint32_t)Op::STORE, (uint32_t)src,
                                (uint32_t)FP, (uint32_t)a->localOff()});
            }
        } else {
            code.push_back({(uint32_t)Op::STORE_VAR, 0, (uint32_t)a->globalAddr(), (uint32_t)src});
        }
        _lines.push_back(a->line);
        freeReg(src);

    } else if (auto is = std::dynamic_pointer_cast<IfStmt>(stmt)) {
        auto condChunk = genExpr(postOrder(is->cond()));
        rebaseJumps(condChunk, (uint16_t)code.size());
        code.insert(code.end(), condChunk.begin(), condChunk.end());
        addLines(is->line, condChunk.size());
        int condR = condChunk.empty() ? 0 : (int)condChunk.back().dst;

        size_t jzIdx = code.size();
        code.push_back({(uint32_t)Op::JZ, (uint32_t)condR, 0, 0});
        _lines.push_back(is->line);
        freeReg(condR);

        compileStmt(is->thenBr(), code);

        size_t jmpIdx = code.size();
        code.push_back({(uint32_t)Op::JMP, 0, 0, 0});
        _lines.push_back(is->line);

        setAddr(code[jzIdx], (uint16_t)code.size());
        if (is->elseBr()) compileStmt(is->elseBr(), code);
        setAddr(code[jmpIdx], (uint16_t)code.size());

    } else if (auto ws = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
        _breakStack.push({}); _contStack.push({});
        size_t start = code.size();
        auto condChunk = genExpr(postOrder(ws->cond()));
        rebaseJumps(condChunk, (uint16_t)code.size());
        code.insert(code.end(), condChunk.begin(), condChunk.end());
        addLines(ws->line, condChunk.size());
        int condR = condChunk.empty() ? 0 : (int)condChunk.back().dst;

        size_t jzIdx = code.size();
        code.push_back({(uint32_t)Op::JZ, (uint32_t)condR, 0, 0});
        _lines.push_back(ws->line);
        freeReg(condR);

        compileStmt(ws->body(), code);

        Instr jmpBack{}; jmpBack.op = (uint32_t)Op::JMP;
        setAddr(jmpBack, (uint16_t)start);
        code.push_back(jmpBack); _lines.push_back(ws->line);

        size_t after = code.size();
        setAddr(code[jzIdx], (uint16_t)after);

        for (auto bi : _breakStack.top()) setAddr(code[bi], (uint16_t)after);
        for (auto ci : _contStack.top())  setAddr(code[ci], (uint16_t)start);
        _breakStack.pop(); _contStack.pop();

    } else if (auto blk = std::dynamic_pointer_cast<Block>(stmt)) {
        for (auto& s : blk->stmts()) compileStmt(s, code);

    } else if (auto ps = std::dynamic_pointer_cast<PrintStmt>(stmt)) {
        for (const auto& e : ps->exprs()) {
            if (auto sn = std::dynamic_pointer_cast<StrNode>(e)) {
                int idx = strIdx(sn->val());
                code.push_back({(uint32_t)Op::PRINT_STR, (uint32_t)idx, 0, 0});
                _lines.push_back(ps->line);
            } else {
                auto chunk = genExpr(postOrder(e));
                rebaseJumps(chunk, (uint16_t)code.size());
                code.insert(code.end(), chunk.begin(), chunk.end());
                addLines(ps->line, chunk.size());
                int r = chunk.empty() ? 0 : (int)chunk.back().dst;
                code.push_back({(uint32_t)Op::PRINT, (uint32_t)r, 0, 0});
                _lines.push_back(ps->line);
                freeReg(r);
            }
        }

    } else if (auto fs = std::dynamic_pointer_cast<ForStmt>(stmt)) {
        _breakStack.push({}); _contStack.push({});
        compileStmt(fs->init(), code);
        size_t start = code.size();

        auto condChunk = genExpr(postOrder(fs->cond()));
        rebaseJumps(condChunk, (uint16_t)code.size());
        code.insert(code.end(), condChunk.begin(), condChunk.end());
        addLines(fs->line, condChunk.size());
        int condR = condChunk.empty() ? 0 : (int)condChunk.back().dst;

        size_t jzIdx = code.size();
        code.push_back({(uint32_t)Op::JZ, (uint32_t)condR, 0, 0});
        _lines.push_back(fs->line);
        freeReg(condR);

        compileStmt(fs->body(), code);

        size_t updateAddr = code.size();
        for (auto ci : _contStack.top()) setAddr(code[ci], (uint16_t)updateAddr);

        compileStmt(fs->update(), code);

        Instr jmpFor{}; jmpFor.op = (uint32_t)Op::JMP;
        setAddr(jmpFor, (uint16_t)start);
        code.push_back(jmpFor); _lines.push_back(fs->line);

        size_t after = code.size();
        setAddr(code[jzIdx], (uint16_t)after);
        for (auto bi : _breakStack.top()) setAddr(code[bi], (uint16_t)after);
        _breakStack.pop(); _contStack.pop();

    } else if (auto fd = std::dynamic_pointer_cast<FnDefStmt>(stmt)) {
        if (_fns.count(fd->name()))
            throw std::runtime_error("function '" + fd->name() + "' already defined");

        size_t jmpIdx = code.size();
        code.push_back({(uint32_t)Op::JMP, 0, 0, 0}); _lines.push_back(fd->line);

        size_t fnAddr = code.size();
        _fns[fd->name()] = {fnAddr, (int)fd->params().size()};

        emitPrologue(code, fd->slots());

        for (int i = 0; i < (int)fd->params().size(); ++i) {
            int32_t off = -4 * (i + 1);
            int r = allocReg();
            code.push_back({(uint32_t)Op::LOAD_PARAM, (uint32_t)r, (uint32_t)i, 0});
            _lines.push_back(fd->line);
            code.push_back({(uint32_t)Op::STORE, (uint32_t)r, (uint32_t)FP, (uint32_t)off});
            _lines.push_back(fd->line);
            freeReg(r);
        }

        compileStmt(fd->body(), code);

        if (fd->isVoid()) {
            code.push_back({(uint32_t)Op::RETURN, 0, 0, 0}); _lines.push_back(fd->line);
        }
        setAddr(code[jmpIdx], (uint16_t)code.size());

    } else if (auto cs = std::dynamic_pointer_cast<CallStmt>(stmt)) {
        auto call = cs->call();
        int builtinReg = 0;
        size_t before = code.size();
        if (tryBuiltin(call->name(), call->args(), code, builtinReg)) {
            addLines(cs->line, code.size() - before);
            freeReg(builtinReg);
            return;
        }
        for (const auto& arg : call->args()) {
            auto chunk = genExpr(postOrder(arg));
            rebaseJumps(chunk, (uint16_t)code.size());
            code.insert(code.end(), chunk.begin(), chunk.end());
            addLines(cs->line, chunk.size());
            int ar = chunk.empty() ? 0 : (int)chunk.back().dst;
            code.push_back({(uint32_t)Op::PUSH_ARG, (uint32_t)ar, 0, 0});
            _lines.push_back(cs->line);
            freeReg(ar);
        }
        int dst = allocReg();
        Instr ci{}; ci.op = (uint32_t)Op::CALL; ci.dst = (uint32_t)dst;
        if (_fns.count(call->name())) {
            setAddr(ci, (uint16_t)_fns[call->name()].addr);
        } else {
            setAddr(ci, 0);
            _fwdCalls.push_back({code.size(), call->name()});
        }
        code.push_back(ci); _lines.push_back(cs->line);
        freeReg(dst);

    } else if (auto rs = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
        if (rs->expr()) {
            auto chunk = genExpr(postOrder(rs->expr()));
            rebaseJumps(chunk, (uint16_t)code.size());
            code.insert(code.end(), chunk.begin(), chunk.end());
            addLines(rs->line, chunk.size());
            int r = chunk.empty() ? 0 : (int)chunk.back().dst;
            code.push_back({(uint32_t)Op::RETURN, (uint32_t)r, 0, 0});
            _lines.push_back(rs->line);
        } else {
            code.push_back({(uint32_t)Op::RETURN, 0, 0, 0}); _lines.push_back(rs->line);
        }

    } else if (std::dynamic_pointer_cast<BreakStmt>(stmt)) {
        size_t idx = code.size();
        code.push_back({(uint32_t)Op::JMP, 0, 0, 0}); _lines.push_back(stmt->line);
        _breakStack.top().push_back(idx);

    } else if (std::dynamic_pointer_cast<ContinueStmt>(stmt)) {
        size_t idx = code.size();
        code.push_back({(uint32_t)Op::JMP, 0, 0, 0}); _lines.push_back(stmt->line);
        _contStack.top().push_back(idx);

    } else if (auto sw = std::dynamic_pointer_cast<SwitchStmt>(stmt)) {
        _breakStack.push({});
        auto exprChunk = genExpr(postOrder(sw->expr()));
        rebaseJumps(exprChunk, (uint16_t)code.size());
        code.insert(code.end(), exprChunk.begin(), exprChunk.end());
        addLines(sw->line, exprChunk.size());
        int swReg = exprChunk.empty() ? 0 : (int)exprChunk.back().dst;

        const auto& cases = sw->cases();
        size_t nc = cases.size();
        std::vector<size_t> caseStarts(nc);
        std::vector<std::vector<size_t>> bodyJumps(nc);
        std::vector<size_t> nextJumps;

        for (size_t i = 0; i < nc; ++i) {
            caseStarts[i] = code.size();
            for (const auto& val : cases[i].vals) {
                auto vc = genExpr(postOrder(val));
                rebaseJumps(vc, (uint16_t)code.size());
                code.insert(code.end(), vc.begin(), vc.end());
                addLines(sw->line, vc.size());
                int vr = vc.empty() ? 0 : (int)vc.back().dst;

                int cmpR = allocReg();
                code.push_back({(uint32_t)Op::EQ, (uint32_t)cmpR, (uint32_t)swReg, (uint32_t)vr});
                _lines.push_back(sw->line);
                size_t jzIdx2 = code.size();
                code.push_back({(uint32_t)Op::JZ, (uint32_t)cmpR, 0, 0}); _lines.push_back(sw->line);
                size_t jmpBody = code.size();
                code.push_back({(uint32_t)Op::JMP, 0, 0, 0}); _lines.push_back(sw->line);
                setAddr(code[jzIdx2], (uint16_t)code.size());
                bodyJumps[i].push_back(jmpBody);
                freeReg(vr); freeReg(cmpR);
            }
            size_t nextJmp = code.size();
            code.push_back({(uint32_t)Op::JMP, 0, 0, 0}); _lines.push_back(sw->line);
            nextJumps.push_back(nextJmp);
        }

        std::vector<size_t> bodyAddrs(nc);
        for (size_t i = 0; i < nc; ++i) {
            bodyAddrs[i] = code.size();
            for (auto jmp : bodyJumps[i]) setAddr(code[jmp], (uint16_t)bodyAddrs[i]);
            compileStmt(cases[i].body, code);
        }

        size_t defAddr = code.size();
        if (sw->defBody()) compileStmt(sw->defBody(), code);
        size_t endAddr = code.size();

        for (size_t i = 0; i < nc; ++i) {
            size_t target = (i + 1 < nc) ? caseStarts[i + 1] : (sw->defBody() ? defAddr : endAddr);
            setAddr(code[nextJumps[i]], (uint16_t)target);
        }
        for (auto bi : _breakStack.top()) setAddr(code[bi], (uint16_t)endAddr);
        _breakStack.pop();
        freeReg(swReg);
    }
}

// ─── Main compile entry ──────────────────────────────────────────────────────

ByteCode Compiler::compile(std::shared_ptr<ASTNode> root) {
    _constPool.clear(); _strPool.clear();
    _constMap.clear();  _strMap.clear();
    _lines.clear(); _fns.clear(); _fwdCalls.clear();
    _nextReg = 0;
    while (!_freeRegs.empty()) _freeRegs.pop();

    std::vector<Instr> instrs;
    if (!root) {
        ByteCode empty;
        empty.globalSlots = _sym.globalSlotCount();
        return empty;
    }

    auto optimized = optimize(root);
    emitPrologue(instrs, _sym.programSlots());

    if (auto blk = std::dynamic_pointer_cast<Block>(optimized)) {
        for (auto& s : blk->stmts()) compileStmt(s, instrs);
    } else if (auto s = std::dynamic_pointer_cast<StmtNode>(optimized)) {
        compileStmt(s, instrs);
    }

    // Call main
    auto mainIt = _fns.find("main");
    if (mainIt == _fns.end())
        throw std::runtime_error("Program must define void fn main()");

    int retReg = allocReg();
    Instr callMain{}; callMain.op = (uint32_t)Op::CALL; callMain.dst = (uint32_t)retReg;
    setAddr(callMain, (uint16_t)mainIt->second.addr);
    instrs.push_back(callMain); _lines.push_back(0);
    freeReg(retReg);

    // Patch forward calls
    for (auto& [idx, name] : _fwdCalls) {
        if (!_fns.count(name))
            throw std::runtime_error("undefined function: " + name);
        setAddr(instrs[idx], (uint16_t)_fns[name].addr);
    }

    while (_lines.size() < instrs.size()) _lines.push_back(_lines.empty() ? 0 : _lines.back());

    ByteCode bc;
    bc.instrs = std::move(instrs);
    bc.consts  = _constPool;
    bc.strings = _strPool;
    bc.lines   = std::move(_lines);
    for (auto& [n, fi] : _fns) bc.fnSymbols[n] = fi.addr;
    bc.globalSlots = _sym.globalSlotCount();
    bc.globalNames.assign(bc.globalSlots, {});
    for (auto& [n, a] : _sym.globals())
        if (a < bc.globalNames.size()) bc.globalNames[a] = n;
    return bc;
}

// ─── Bytecode file I/O ───────────────────────────────────────────────────────

void writeBytecode(const ByteCode& bc, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open '" + path + "' for writing");

    // Magic "NGOB"
    const char magic[4] = {'N','G','O','B'};
    out.write(magic, 4);

    auto w32 = [&](uint32_t v) { out.write((char*)&v, 4); };
    auto w64 = [&](double  v) { out.write((char*)&v, 8); };

    w32((uint32_t)bc.instrs.size());
    w32((uint32_t)bc.consts.size());
    w32((uint32_t)bc.strings.size());
    w32((uint32_t)bc.lines.size());

    for (int ln : bc.lines) w32((uint32_t)ln);

    for (const auto& i : bc.instrs) {
        out.put((char)i.op); out.put((char)i.dst);
        out.put((char)i.left); out.put((char)i.right);
    }
    for (double v : bc.consts) w64(v);

    for (const auto& s : bc.strings) {
        w32((uint32_t)s.size());
        out.write(s.data(), (std::streamsize)s.size());
    }

    w32((uint32_t)bc.globalSlots);
    for (uint32_t i = 0; i < (uint32_t)bc.globalSlots; ++i) {
        const std::string& n = (i < bc.globalNames.size()) ? bc.globalNames[i] : std::string{};
        w32((uint32_t)n.size());
        if (!n.empty()) out.write(n.data(), (std::streamsize)n.size());
    }

    if (!out.good()) throw std::runtime_error("write failed: " + path);
}

ByteCode readBytecode(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open '" + path + "'");

    char magic[4] = {};
    in.read(magic, 4);
    if (std::memcmp(magic, "NGOB", 4) != 0)
        throw std::runtime_error("invalid bytecode file: " + path);

    auto r32 = [&]() -> uint32_t { uint32_t v = 0; in.read((char*)&v, 4); return v; };
    auto r64 = [&]() -> double   { double   v = 0; in.read((char*)&v, 8); return v; };

    uint32_t ni = r32(), nc = r32(), ns = r32(), nl = r32();

    ByteCode bc;
    bc.lines.resize(nl);
    for (auto& l : bc.lines) l = (int)r32();

    bc.instrs.reserve(ni);
    for (uint32_t i = 0; i < ni; ++i) {
        Instr inst{};
        inst.op    = (uint8_t)in.get();
        inst.dst   = (uint8_t)in.get();
        inst.left  = (uint8_t)in.get();
        inst.right = (uint8_t)in.get();
        bc.instrs.push_back(inst);
    }
    bc.consts.resize(nc);
    for (auto& c : bc.consts) c = r64();

    bc.strings.reserve(ns);
    for (uint32_t i = 0; i < ns; ++i) {
        uint32_t len = r32();
        std::string s(len, '\0');
        if (len) in.read(&s[0], len);
        bc.strings.push_back(std::move(s));
    }

    bc.globalSlots = r32();
    bc.globalNames.resize(bc.globalSlots);
    for (auto& n : bc.globalNames) {
        uint32_t len = r32();
        n.resize(len);
        if (len) in.read(&n[0], len);
    }

    if (!in.good() && !in.eof())
        throw std::runtime_error("read error: " + path);
    return bc;
}
