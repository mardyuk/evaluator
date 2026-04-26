#pragma once
#include <stack>
#include <memory>
#include <string>
#include "tokenizer.hpp"
#include "ast.hpp"
#include "scope.hpp"

class Parser {
public:
    Parser(Tokenizer& tok, SymbolTable& st);
    std::shared_ptr<StmtNode> parseProgram();

private:
    Tokenizer&   _tok;
    SymbolTable& _sym;
    Token        _cur;

    bool _inFn     = false;
    bool _inLoop   = false;
    bool _inSwitch = false;
    bool _hasMain  = false;

    void next() { _cur = _tok.next(); }
    void error(const std::string& msg) const;

    // ─── Statement parsing ───────────────────────────────────────────────────
    std::shared_ptr<StmtNode> parseStatement();
    std::shared_ptr<StmtNode> parseBlock();
    std::shared_ptr<StmtNode> parseIf();
    std::shared_ptr<StmtNode> parseWhile();
    std::shared_ptr<StmtNode> parseFor();
    std::shared_ptr<StmtNode> parsePrint();
    std::shared_ptr<StmtNode> parseReturn();
    std::shared_ptr<StmtNode> parseSwitch();
    std::shared_ptr<StmtNode> parseFnDef();

    // Variable declarations  (let / let global / let local)
    std::shared_ptr<StmtNode> parseLetDecl();
    // Assignment / compound-assignment for a known name
    std::shared_ptr<StmtNode> parseAssign(const std::string& name, bool freshDecl,
                                           bool forceLocal, bool forceGlobal);
    // Multiple-variable  let a, b, c = 1, 2, 3
    std::shared_ptr<StmtNode> parseLetList(bool forceLocal, bool forceGlobal);

    // ─── Expression parsing (shunting-yard) ──────────────────────────────────
    std::shared_ptr<ASTNode> parseExpr();
    std::shared_ptr<ASTNode> parseCallOrBuiltin(const std::string& name);

    // Shunting-yard helpers
    int  prec(const std::string& op) const;
    bool isRightAssoc(const std::string& op) const;
    void pushOp(const std::string& op,
                std::stack<std::string>& ops,
                std::stack<std::shared_ptr<ASTNode>>& vals);
    void applyOp(const std::string& op,
                 std::stack<std::shared_ptr<ASTNode>>& vals);

    // Constant folding during parse
    std::shared_ptr<ASTNode> foldBin(const std::string& op,
                                      std::shared_ptr<ASTNode> l,
                                      std::shared_ptr<ASTNode> r);
    std::shared_ptr<ASTNode> foldUnary(const std::string& op,
                                        std::shared_ptr<ASTNode> child);

    // Scoping helpers
    bool defaultToLocal(bool explicitGlobal) const;
    bool isTopLevel() const;
    void checkTopLevel(const std::shared_ptr<StmtNode>& s, int line) const;

    void expect(TokType t, const std::string& msg);
    bool check(TokType t) const { return _cur.type == t; }
};
