#include "vm.hpp"
#include "value.hpp"
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cstdlib>
#include <ctime>

static const char* OP_NAMES[] = {
    "ADD","SUB","MUL","DIV","MOD","POW","FDIV","FRACDIV",
    "BAND","BOR","BXOR","BNOT","SHL","SHR",
    "EQ","NEQ","LT","GT","LEQ","GEQ",
    "AND","OR","LNOT","NEG",
    "LOAD_CONST","LOAD_VAR","LOAD_STR","LOAD_NONE",
    "STORE_VAR","LOAD","STORE","LOAD_OUTER","STORE_OUTER","MOV",
    "JMP","JZ","CALL","RETURN","PUSH_ARG","LOAD_PARAM",
    "PRINT","PRINT_STR","ADDI",
    "SIN","COS","TAN","ASIN","ACOS","ATAN","ATAN2",
    "SQRT","CBRT","EXP","LOG","LOG10","LOG2","LOG_AB","MATH_POW",
    "CEIL","FLOOR","ABS","ROUND","FMOD",
    "CONST_PI","CONST_E","CONST_INF","CONST_MAX",
    "INPUT","LENGTH","TYPE","CHR","ORD","BIN","OCT","HEX","DEC",
    "STR_GET","RANDOM"
};

void VM::initFromBytecode(const ByteCode& bc) {
    _prog        = bc.instrs;
    _consts      = bc.consts;
    _strs        = bc.strings;
    _lines       = bc.lines;
    _globalSlots = bc.globalSlots;
    _globalNames = bc.globalNames;
    _globalDefined.assign(_globalSlots, false);
    _callStack.clear();
    _argBuf.clear();

    _regs[2] = Value{10000.0};   // SP
    _regs[8] = Value{10000.0};   // FP
}

void VM::load(const ByteCode& bc) {
    initFromBytecode(bc);
}

void VM::loadFile(const std::string& path) {
    initFromBytecode(readBytecode(path));
}

void VM::showInstruction(size_t pc) const {
    if (pc >= _prog.size()) return;
    const auto& ins = _prog[pc];
    int ln = (pc < _lines.size()) ? _lines[pc] : -1;
    std::cerr << std::right << std::setw(5) << pc
              << "  [line " << std::setw(3) << ln << "]  ";
    const char* name = (ins.op < sizeof(OP_NAMES)/sizeof(*OP_NAMES))
                       ? OP_NAMES[ins.op] : "???";
    std::cerr << std::left  << std::setw(13) << name
              << "  dst=" << ins.dst
              << "  left=" << ins.left
              << "  right=" << ins.right << "\n";
}

void VM::debugPrompt(size_t pc) {
    showInstruction(pc);
    while (true) {
        std::cerr << "(dbg) ";
        std::string line;
        if (!std::getline(std::cin, line)) { _step = false; return; }
        if (line.empty() || line == "s") return;
        if (line == "g") { _step = false; return; }
        if (line == "q") throw std::runtime_error("debugger quit");
        if (line.size() >= 2 && line[0] == 'r') {
            int reg = std::stoi(line.substr(1));
            if (reg >= 0 && reg < (int)_regs.size())
                std::cerr << "  r" << reg << " = " << valStr(_regs[reg]) << "\n";
        } else if (line.size() >= 2 && line[0] == 'm') {
            int addr = std::stoi(line.substr(1));
            if (addr >= 0 && addr < (int)_mem.size())
                std::cerr << "  mem[" << addr << "] = " << valStr(_mem[addr]) << "\n";
        } else {
            std::cerr << "  s/enter=step  g=go  q=quit  r<N>=reg  m<N>=mem\n";
        }
    }
}

double VM::run() {
    static bool seeded = false;
    if (!seeded) { std::srand((unsigned)std::time(nullptr)); seeded = true; }
    if (_debug) _step = true;
    size_t pc = 0;

    try {
        while (pc < _prog.size()) {
            if (_debug && _step) debugPrompt(pc);

            const Instr& ins = _prog[pc];
            Op op = (Op)ins.op;

            switch (op) {

            // ── Arithmetic ────────────────────────────────────────────────────

            case Op::ADD: {
                auto& lv = _regs[ins.left];
                auto& rv = _regs[ins.right];
                if (std::holds_alternative<std::string>(lv) ||
                    std::holds_alternative<std::string>(rv))
                    _regs[ins.dst] = Value{valStr(lv) + valStr(rv)};
                else
                    _regs[ins.dst] = Value{asNum(lv) + asNum(rv)};
                break;
            }
            case Op::SUB:
                _regs[ins.dst] = Value{asNum(_regs[ins.left]) - asNum(_regs[ins.right])};
                break;
            case Op::MUL:
                _regs[ins.dst] = Value{asNum(_regs[ins.left]) * asNum(_regs[ins.right])};
                break;
            case Op::DIV: {
                double b = asNum(_regs[ins.right]);
                if (b == 0.0) throw std::runtime_error("division by zero");
                _regs[ins.dst] = Value{asNum(_regs[ins.left]) / b};
                break;
            }
            case Op::MOD: {
                long long lv = (long long)asNum(_regs[ins.left]);
                long long rv = (long long)asNum(_regs[ins.right]);
                if (rv == 0) throw std::runtime_error("modulo by zero");
                _regs[ins.dst] = Value{(double)(lv % rv)};
                break;
            }
            case Op::POW:
                _regs[ins.dst] = Value{std::pow(asNum(_regs[ins.left]), asNum(_regs[ins.right]))};
                break;
            case Op::FDIV: {
                double b = asNum(_regs[ins.right]);
                if (b == 0.0) throw std::runtime_error("floor division by zero");
                _regs[ins.dst] = Value{std::floor(asNum(_regs[ins.left]) / b)};
                break;
            }
            case Op::FRACDIV: {
                double a = asNum(_regs[ins.left]), b = asNum(_regs[ins.right]);
                if (b == 0.0) throw std::runtime_error("division by zero");
                _regs[ins.dst] = Value{std::fmod(a, b)};
                break;
            }

            // ── Bitwise ───────────────────────────────────────────────────────

            case Op::BAND: {
                long long l = (long long)asNum(_regs[ins.left]);
                long long r = (long long)asNum(_regs[ins.right]);
                _regs[ins.dst] = Value{(double)(l & r)};
                break;
            }
            case Op::BOR: {
                long long l = (long long)asNum(_regs[ins.left]);
                long long r = (long long)asNum(_regs[ins.right]);
                _regs[ins.dst] = Value{(double)(l | r)};
                break;
            }
            case Op::BXOR: {
                long long l = (long long)asNum(_regs[ins.left]);
                long long r = (long long)asNum(_regs[ins.right]);
                _regs[ins.dst] = Value{(double)(l ^ r)};
                break;
            }
            case Op::BNOT: {
                long long v = (long long)asNum(_regs[ins.left]);
                _regs[ins.dst] = Value{(double)(~v)};
                break;
            }
            case Op::SHL: {
                long long l = (long long)asNum(_regs[ins.left]);
                int r = (int)asNum(_regs[ins.right]) & 0x3F;
                _regs[ins.dst] = Value{(double)(l << r)};
                break;
            }
            case Op::SHR: {
                unsigned long long l = (unsigned long long)(long long)asNum(_regs[ins.left]);
                int r = (int)asNum(_regs[ins.right]) & 0x3F;
                _regs[ins.dst] = Value{(double)(l >> r)};
                break;
            }

            // ── Comparison ────────────────────────────────────────────────────

            case Op::EQ: {
                const auto& lv = _regs[ins.left];
                const auto& rv = _regs[ins.right];
                bool eq = false;
                if (lv.index() == rv.index()) {
                    if (std::holds_alternative<double>(lv))
                        eq = std::get<double>(lv) == std::get<double>(rv);
                    else if (std::holds_alternative<std::string>(lv))
                        eq = std::get<std::string>(lv) == std::get<std::string>(rv);
                    else
                        eq = true; // both none
                }
                _regs[ins.dst] = Value{eq ? 1.0 : 0.0};
                break;
            }
            case Op::NEQ: {
                const auto& lv = _regs[ins.left];
                const auto& rv = _regs[ins.right];
                bool eq = false;
                if (lv.index() == rv.index()) {
                    if (std::holds_alternative<double>(lv))
                        eq = std::get<double>(lv) == std::get<double>(rv);
                    else if (std::holds_alternative<std::string>(lv))
                        eq = std::get<std::string>(lv) == std::get<std::string>(rv);
                    else
                        eq = true;
                }
                _regs[ins.dst] = Value{eq ? 0.0 : 1.0};
                break;
            }
            case Op::LT:
                _regs[ins.dst] = Value{asNum(_regs[ins.left]) < asNum(_regs[ins.right]) ? 1.0 : 0.0};
                break;
            case Op::GT:
                _regs[ins.dst] = Value{asNum(_regs[ins.left]) > asNum(_regs[ins.right]) ? 1.0 : 0.0};
                break;
            case Op::LEQ:
                _regs[ins.dst] = Value{asNum(_regs[ins.left]) <= asNum(_regs[ins.right]) ? 1.0 : 0.0};
                break;
            case Op::GEQ:
                _regs[ins.dst] = Value{asNum(_regs[ins.left]) >= asNum(_regs[ins.right]) ? 1.0 : 0.0};
                break;

            // ── Logical ───────────────────────────────────────────────────────

            case Op::AND:
                _regs[ins.dst] = Value{(isTruthy(_regs[ins.left]) && isTruthy(_regs[ins.right])) ? 1.0 : 0.0};
                break;
            case Op::OR:
                _regs[ins.dst] = Value{(isTruthy(_regs[ins.left]) || isTruthy(_regs[ins.right])) ? 1.0 : 0.0};
                break;
            case Op::LNOT:
                _regs[ins.dst] = Value{isFalsy(_regs[ins.left]) ? 1.0 : 0.0};
                break;
            case Op::NEG:
                _regs[ins.dst] = Value{-asNum(_regs[ins.left])};
                break;

            // ── Memory ────────────────────────────────────────────────────────

            case Op::LOAD_CONST:
                _regs[ins.dst] = Value{_consts[ins.left]};
                break;
            case Op::LOAD_VAR:
                _regs[ins.dst] = _mem[ins.left];
                break;
            case Op::LOAD_STR:
                _regs[ins.dst] = Value{_strs[ins.left]};
                break;
            case Op::LOAD_NONE:
                _regs[ins.dst] = Value{std::monostate{}};
                break;
            case Op::STORE_VAR:
                _mem[ins.left] = _regs[ins.right];
                if (ins.left < _globalDefined.size()) _globalDefined[ins.left] = true;
                break;
            case Op::LOAD: {
                int base = (int)asNum(_regs[ins.left]);
                int off  = (int)(int8_t)ins.right;
                _regs[ins.dst] = _mem[base + off];
                break;
            }
            case Op::STORE: {
                int base = (int)asNum(_regs[ins.left]);
                int off  = (int)(int8_t)ins.right;
                _mem[base + off] = _regs[ins.dst];
                break;
            }
            case Op::LOAD_OUTER: {
                int hops = (int)ins.left;
                int off  = (int)(int8_t)ins.right;
                int fpIdx = (int)asNum(_callStack[_callStack.size() - hops].callerFp);
                _regs[ins.dst] = _mem[fpIdx + off];
                break;
            }
            case Op::STORE_OUTER: {
                int hops = (int)ins.left;
                int off  = (int)(int8_t)ins.right;
                int fpIdx = (int)asNum(_callStack[_callStack.size() - hops].callerFp);
                _mem[fpIdx + off] = _regs[ins.dst];
                break;
            }
            case Op::MOV:
                _regs[ins.dst] = _regs[ins.left];
                break;

            // ── Control flow ──────────────────────────────────────────────────

            case Op::JMP:
                pc = getAddr(ins);
                continue;
            case Op::JZ:
                if (isFalsy(_regs[ins.dst])) {
                    pc = getAddr(ins);
                    continue;
                }
                break;

            // ── Functions ─────────────────────────────────────────────────────

            case Op::CALL: {
                CallFrame frame;
                frame.retAddr   = pc + 1;
                frame.retDst    = ins.dst;
                frame.callerSp  = _regs[2];
                frame.callerFp  = _regs[8];
                frame.args      = std::move(_argBuf);
                frame.savedRegs = _regs;   // save full register file
                _argBuf.clear();
                _callStack.push_back(std::move(frame));
                pc = getAddr(ins);
                continue;
            }
            case Op::RETURN: {
                Value retVal = _regs[ins.dst];
                if (_callStack.empty()) {
                    double r = std::holds_alternative<double>(retVal) ? std::get<double>(retVal) : 0.0;
                    return r;
                }
                CallFrame frame = std::move(_callStack.back());
                _callStack.pop_back();
                _regs = std::move(frame.savedRegs);   // restore caller registers
                _regs[frame.retDst] = retVal;          // put return value in place
                pc = frame.retAddr;
                continue;
            }
            case Op::PUSH_ARG:
                _argBuf.push_back(_regs[ins.dst]);
                break;
            case Op::LOAD_PARAM:
                _regs[ins.dst] = _callStack.back().args[ins.left];
                break;

            // ── I/O ───────────────────────────────────────────────────────────

            case Op::PRINT: {
                const auto& v = _regs[ins.dst];
                if (std::holds_alternative<std::string>(v))
                    std::cout << std::get<std::string>(v);
                else if (std::holds_alternative<double>(v))
                    std::cout << valStr(v);
                else
                    std::cout << "none";
                break;
            }
            case Op::PRINT_STR:
                std::cout << _strs[ins.dst];
                break;

            // ── Frame setup ───────────────────────────────────────────────────

            case Op::ADDI: {
                double base = asNum(_regs[ins.left]);
                double imm  = (double)(int8_t)ins.right;
                _regs[ins.dst] = Value{base + imm};
                break;
            }

            // ── Math functions ────────────────────────────────────────────────

            case Op::SIN:      _regs[ins.dst] = Value{std::sin(asNum(_regs[ins.left]))}; break;
            case Op::COS:      _regs[ins.dst] = Value{std::cos(asNum(_regs[ins.left]))}; break;
            case Op::TAN:      _regs[ins.dst] = Value{std::tan(asNum(_regs[ins.left]))}; break;
            case Op::ASIN:     _regs[ins.dst] = Value{std::asin(asNum(_regs[ins.left]))}; break;
            case Op::ACOS:     _regs[ins.dst] = Value{std::acos(asNum(_regs[ins.left]))}; break;
            case Op::ATAN:     _regs[ins.dst] = Value{std::atan(asNum(_regs[ins.left]))}; break;
            case Op::ATAN2:    _regs[ins.dst] = Value{std::atan2(asNum(_regs[ins.left]), asNum(_regs[ins.right]))}; break;
            case Op::SQRT:     _regs[ins.dst] = Value{std::sqrt(asNum(_regs[ins.left]))}; break;
            case Op::CBRT:     _regs[ins.dst] = Value{std::cbrt(asNum(_regs[ins.left]))}; break;
            case Op::EXP:      _regs[ins.dst] = Value{std::exp(asNum(_regs[ins.left]))}; break;
            case Op::LOG:      _regs[ins.dst] = Value{std::log(asNum(_regs[ins.left]))}; break;
            case Op::LOG10:    _regs[ins.dst] = Value{std::log10(asNum(_regs[ins.left]))}; break;
            case Op::LOG2:     _regs[ins.dst] = Value{std::log2(asNum(_regs[ins.left]))}; break;
            case Op::LOG_AB: {
                double base = asNum(_regs[ins.left]);
                double x    = asNum(_regs[ins.right]);
                _regs[ins.dst] = Value{std::log(x) / std::log(base)};
                break;
            }
            case Op::MATH_POW: _regs[ins.dst] = Value{std::pow(asNum(_regs[ins.left]), asNum(_regs[ins.right]))}; break;
            case Op::CEIL:     _regs[ins.dst] = Value{std::ceil(asNum(_regs[ins.left]))}; break;
            case Op::FLOOR:    _regs[ins.dst] = Value{std::floor(asNum(_regs[ins.left]))}; break;
            case Op::ABS:      _regs[ins.dst] = Value{std::abs(asNum(_regs[ins.left]))}; break;
            case Op::ROUND:    _regs[ins.dst] = Value{std::round(asNum(_regs[ins.left]))}; break;
            case Op::FMOD:     _regs[ins.dst] = Value{std::fmod(asNum(_regs[ins.left]), asNum(_regs[ins.right]))}; break;

            // ── Math constants ────────────────────────────────────────────────

            case Op::CONST_PI:  _regs[ins.dst] = Value{M_PI}; break;
            case Op::CONST_E:   _regs[ins.dst] = Value{M_E}; break;
            case Op::CONST_INF: _regs[ins.dst] = Value{std::numeric_limits<double>::infinity()}; break;
            case Op::CONST_MAX: _regs[ins.dst] = Value{std::numeric_limits<double>::max()}; break;

            // ── Built-in functions ────────────────────────────────────────────

            case Op::INPUT: {
                std::string raw;
                if (!std::getline(std::cin, raw)) {
                    _regs[ins.dst] = Value{std::monostate{}}; break;
                }
                // strip \r for Windows line endings
                if (!raw.empty() && raw.back() == '\r') raw.pop_back();
                // trim whitespace
                size_t s = raw.find_first_not_of(" \t");
                if (s == std::string::npos) { _regs[ins.dst] = Value{std::monostate{}}; break; }
                size_t e = raw.find_last_not_of(" \t");
                std::string trimmed = raw.substr(s, e - s + 1);
                // quoted string → strip quotes, return as str
                if (trimmed.size() >= 2) {
                    char f = trimmed.front(), b = trimmed.back();
                    if ((f == '"' && b == '"') || (f == '\'' && b == '\'')) {
                        _regs[ins.dst] = Value{trimmed.substr(1, trimmed.size() - 2)};
                        break;
                    }
                }
                // try numeric conversion
                char* end = nullptr;
                double num = std::strtod(trimmed.c_str(), &end);
                // skip trailing whitespace after the number
                while (end && *end == ' ') ++end;
                if (end && *end == '\0')
                    _regs[ins.dst] = Value{num};
                else
                    _regs[ins.dst] = Value{trimmed};
                break;
            }
            case Op::LENGTH: {
                const auto& v = _regs[ins.left];
                if (!std::holds_alternative<std::string>(v))
                    throw std::runtime_error("length() requires a string");
                _regs[ins.dst] = Value{(double)std::get<std::string>(v).size()};
                break;
            }
            case Op::TYPE: {
                const auto& v = _regs[ins.left];
                if (std::holds_alternative<double>(v))
                    _regs[ins.dst] = Value{std::string{"num"}};
                else if (std::holds_alternative<std::string>(v))
                    _regs[ins.dst] = Value{std::string{"str"}};
                else
                    _regs[ins.dst] = Value{std::string{"none"}};
                break;
            }
            case Op::CHR: {
                int code = (int)asNum(_regs[ins.left]);
                _regs[ins.dst] = Value{std::string(1, (char)code)};
                break;
            }
            case Op::ORD: {
                const auto& v = _regs[ins.left];
                if (!std::holds_alternative<std::string>(v) || std::get<std::string>(v).empty())
                    throw std::runtime_error("ord() requires a non-empty string");
                _regs[ins.dst] = Value{(double)(unsigned char)std::get<std::string>(v)[0]};
                break;
            }
            case Op::BIN: {
                unsigned long long n = (unsigned long long)(long long)asNum(_regs[ins.left]);
                if (n == 0) { _regs[ins.dst] = Value{std::string{"0b0"}}; break; }
                std::string bits;
                unsigned long long tmp = n;
                while (tmp) { bits = (char)('0' + (tmp & 1)) + bits; tmp >>= 1; }
                _regs[ins.dst] = Value{"0b" + bits};
                break;
            }
            case Op::OCT: {
                long long n = (long long)asNum(_regs[ins.left]);
                std::ostringstream oss;
                oss << "0o" << std::oct << n;
                _regs[ins.dst] = Value{oss.str()};
                break;
            }
            case Op::HEX: {
                long long n = (long long)asNum(_regs[ins.left]);
                std::ostringstream oss;
                oss << "0x" << std::hex << n;
                _regs[ins.dst] = Value{oss.str()};
                break;
            }
            case Op::DEC: {
                const auto& v = _regs[ins.left];
                if (std::holds_alternative<std::string>(v)) {
                    try {
                        _regs[ins.dst] = Value{(double)std::stoull(std::get<std::string>(v), nullptr, 0)};
                    } catch (...) {
                        throw std::runtime_error("dec(): invalid number string");
                    }
                } else {
                    _regs[ins.dst] = Value{(double)(long long)asNum(v)};
                }
                break;
            }
            case Op::STR_GET: {
                const auto& sv = _regs[ins.left];
                if (!std::holds_alternative<std::string>(sv))
                    throw std::runtime_error("string index requires a string");
                const std::string& str = std::get<std::string>(sv);
                int32_t idx = (int32_t)asNum(_regs[ins.right]);
                if (idx < 0) idx += (int32_t)str.size();
                if (idx < 0 || idx >= (int32_t)str.size())
                    throw std::runtime_error("string index out of range");
                _regs[ins.dst] = Value{std::string(1, str[(size_t)idx])};
                break;
            }
            case Op::RANDOM:
                _regs[ins.dst] = Value{(double)std::rand() / ((double)RAND_MAX + 1.0)};
                break;

            default:
                throw std::runtime_error("unknown opcode " + std::to_string(ins.op));
            }

            ++pc;
        }
    } catch (const std::exception& e) {
        int line = (pc < _lines.size()) ? _lines[pc] : -1;
        std::string msg = e.what();
        if (msg.find("[line") == std::string::npos && line > 0)
            throw std::runtime_error("[line " + std::to_string(line) + "] " + msg);
        throw;
    }

    return 0.0;
}
