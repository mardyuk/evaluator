#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include "compiler.hpp"
#include "value.hpp"

struct CallFrame {
    size_t         retAddr;
    size_t         retDst;
    Value          callerSp;
    Value          callerFp;
    std::vector<Value> args;
    std::vector<Value> savedRegs;
};

class VM {
public:
    explicit VM(bool debug = false) : _debug(debug) {
        _regs.resize(256, Value{0.0});
        _mem.resize(65536, Value{std::monostate{}});
    }

    void load(const ByteCode& bc);
    void loadFile(const std::string& path);
    double run();

private:
    std::vector<Value>   _regs;
    std::vector<Value>   _mem;
    std::vector<Instr>   _prog;
    std::vector<double>  _consts;
    std::vector<std::string> _strs;
    std::vector<int>     _lines;
    bool _debug;

    std::vector<CallFrame> _callStack;
    std::vector<Value>     _argBuf;

    size_t _globalSlots = 0;
    std::vector<std::string> _globalNames;
    std::vector<bool>        _globalDefined;

    // Debug
    bool _step = false;
    bool _go   = false;
    void debugPrompt(size_t pc);

    void showInstruction(size_t pc) const;
    void initFromBytecode(const ByteCode& bc);
};
