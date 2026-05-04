#pragma once
#include <vector>
#include <stack>
#include <string>
#include <unordered_map>
#include <memory>
#include "ast.hpp"
#include "scope.hpp"

struct Instr {
    uint32_t op   : 8;
    uint32_t dst  : 8;
    uint32_t left : 8;
    uint32_t right: 8;
};

inline uint16_t getAddr(const Instr& i) {
    return (uint16_t)((i.right << 8) | i.left);
}
inline void setAddr(Instr& i, uint16_t addr) {
    i.left  =  addr & 0xFF;
    i.right = (addr >> 8) & 0xFF;
}

struct FnInfo {
    size_t addr;
    int    paramCount;
};

struct ByteCode {
    std::vector<Instr>       instrs;
    std::vector<double>      consts;
    std::vector<std::string> strings;
    std::vector<int>         lines;
    std::unordered_map<std::string, size_t> fnSymbols;
    size_t                   globalSlots = 0;
    std::vector<std::string> globalNames; // for runtime error messages
};

void writeBytecode(const ByteCode& bc, const std::string& path);
ByteCode readBytecode(const std::string& path);

class Compiler {
public:
    explicit Compiler(SymbolTable& sym) : _sym(sym) {}

    ByteCode compile(std::shared_ptr<ASTNode> root);
    void compileStmt(std::shared_ptr<StmtNode> stmt, std::vector<Instr>& code);

    std::shared_ptr<ASTNode> optimize(std::shared_ptr<ASTNode> node);

private:
    SymbolTable& _sym;

    int  _nextReg = 0;
    std::stack<int> _freeRegs;

    std::vector<double>      _constPool;
    std::vector<std::string> _strPool;
    std::unordered_map<double,      int> _constMap;
    std::unordered_map<std::string, int> _strMap;

    std::unordered_map<std::string, FnInfo> _fns;
    std::vector<std::pair<size_t, std::string>> _fwdCalls;

    std::stack<std::vector<size_t>> _breakStack;
    std::stack<std::vector<size_t>> _contStack;

    std::vector<int> _lines;

    static const int SP = 2;
    static const int FP = 8;

    int  allocReg();
    void freeReg(int r);

    void emitPrologue(std::vector<Instr>& code, int slots);

    // Post-order traversal for expressions
    std::vector<std::shared_ptr<ASTNode>> postOrder(std::shared_ptr<ASTNode> root);
    std::vector<Instr> genExpr(const std::vector<std::shared_ptr<ASTNode>>& nodes);

    // Built-in math / utility functions
    bool tryBuiltin(const std::string& name,
                    const std::vector<std::shared_ptr<ASTNode>>& args,
                    std::vector<Instr>& code, int& resultReg);

    void rebaseJumps(std::vector<Instr>& chunk, uint16_t base);
    void addLines(int line, size_t count);
    int  constIdx(double v);
    int  strIdx(const std::string& s);
};
