#include "tokenizer.hpp"
#include <cctype>
#include <unordered_map>
#include <unordered_set>

static const std::unordered_map<std::string, TokType> kKeywords = {
    {"if",       TokType::If},
    {"else",     TokType::Else},
    {"while",    TokType::While},
    {"for",      TokType::For},
    {"fn",       TokType::Fn},
    {"void",     TokType::Void},
    {"return",   TokType::Return},
    {"let",      TokType::Let},
    {"global",   TokType::Global},
    {"local",    TokType::Local},
    {"print",    TokType::Print},
    {"true",     TokType::Bool},
    {"false",    TokType::Bool},
    {"none",     TokType::None},
    {"break",    TokType::Break},
    {"continue", TokType::Continue},
    {"switch",   TokType::Switch},
    {"case",     TokType::Case},
    {"default",  TokType::Default},
    {"and",      TokType::And},
    {"or",       TokType::Or},
    {"not",      TokType::Not},
    {"PI",       TokType::MathConst},
    {"E",        TokType::MathConst},
    {"INF",      TokType::MathConst},
    {"MAX",      TokType::MathConst},
};

static const std::unordered_set<std::string> kBuiltins = {
    "sin","cos","tan","asin","acos","atan","atan2",
    "sqrt","cbrt","pow","exp","log","ln","log10","log2","log_ab",
    "ceil","floor","abs","round","fmod",
    "input","length","type","chr","ord","bin","oct","hex","dec",
};

Tokenizer::Tokenizer(Lexer& lex) : _lex(lex) {}

void Tokenizer::skip() {
    while (!_lex.atEnd()) {
        int c = _lex.peek();
        if (std::isspace(c)) { _lex.advance(); continue; }

        // single-line comment: //
        if (c == '/' && _lex.peekNext() == '/') {
            while (!_lex.atEnd() && _lex.peek() != '\n') _lex.advance();
            continue;
        }
        // multi-line comment: /* ... */
        if (c == '/' && _lex.peekNext() == '*') {
            _lex.advance(); // '/'
            _lex.advance(); // '*'
            while (!_lex.atEnd()) {
                if (_lex.peek() == '*' && _lex.peekNext() == '/') {
                    _lex.advance(); _lex.advance(); break;
                }
                _lex.advance();
            }
            continue;
        }
        break;
    }
}

Token Tokenizer::next() {
    skip();
    _lex.markStart();

    if (_lex.atEnd()) return {TokType::Eof, "", _lex.tokLine()};

    int c = _lex.peek();

    if (c == '"' || c == '\'') { _lex.advance(); return readStr((char)c); }

    if (c == '(') { _lex.advance(); return {TokType::LParen,  "(", _lex.tokLine()}; }
    if (c == ')') { _lex.advance(); return {TokType::RParen,  ")", _lex.tokLine()}; }
    if (c == '{') { _lex.advance(); return {TokType::LBrace,  "{", _lex.tokLine()}; }
    if (c == '}') { _lex.advance(); return {TokType::RBrace,  "}", _lex.tokLine()}; }
    if (c == ';') { _lex.advance(); return {TokType::Semi,    ";", _lex.tokLine()}; }
    if (c == ',') { _lex.advance(); return {TokType::Comma,   ",", _lex.tokLine()}; }
    if (c == '?') { _lex.advance(); return {TokType::Question,"?", _lex.tokLine()}; }
    if (c == ':') { _lex.advance(); return {TokType::Colon,   ":", _lex.tokLine()}; }

    if (std::isdigit(c) || (c == '.' && std::isdigit(_lex.peekNext()))) return readNum();
    if (std::isalpha(c) || c == '_') return readName();

    return readOp();
}

Token Tokenizer::readStr(char q) {
    int ln = _lex.tokLine();
    std::string s;
    while (!_lex.atEnd() && _lex.peek() != q) {
        int ch = _lex.peek();
        if (ch == '\\') {
            _lex.advance();
            switch (_lex.peek()) {
                case 'n':  s += '\n'; break;
                case 't':  s += '\t'; break;
                case '"':  s += '"';  break;
                case '\'': s += '\''; break;
                case '\\': s += '\\'; break;
                default:   s += (char)_lex.peek(); break;
            }
        } else {
            s += (char)ch;
        }
        _lex.advance();
    }
    if (!_lex.atEnd()) _lex.advance(); // closing quote
    return {TokType::Str, s, ln};
}

Token Tokenizer::readNum() {
    int ln = _lex.tokLine();
    std::string s;
    bool hasDot = false;

    // Hex prefix 0x / 0X
    if (_lex.peek() == '0' && (_lex.peekNext() == 'x' || _lex.peekNext() == 'X')) {
        s += (char)_lex.peek(); _lex.advance();
        s += (char)_lex.peek(); _lex.advance();
        while (!_lex.atEnd() && std::isxdigit(_lex.peek()))
            { s += (char)_lex.peek(); _lex.advance(); }
        return {TokType::Num, s, ln};
    }
    // Binary prefix 0b / 0B
    if (_lex.peek() == '0' && (_lex.peekNext() == 'b' || _lex.peekNext() == 'B')) {
        s += (char)_lex.peek(); _lex.advance();
        s += (char)_lex.peek(); _lex.advance();
        while (!_lex.atEnd() && (_lex.peek() == '0' || _lex.peek() == '1'))
            { s += (char)_lex.peek(); _lex.advance(); }
        return {TokType::Num, s, ln};
    }
    // Octal prefix 0o / 0O
    if (_lex.peek() == '0' && (_lex.peekNext() == 'o' || _lex.peekNext() == 'O')) {
        s += (char)_lex.peek(); _lex.advance();
        s += (char)_lex.peek(); _lex.advance();
        while (!_lex.atEnd() && _lex.peek() >= '0' && _lex.peek() <= '7')
            { s += (char)_lex.peek(); _lex.advance(); }
        return {TokType::Num, s, ln};
    }

    // Decimal / float with optional _ separator
    while (!_lex.atEnd() && (std::isdigit(_lex.peek()) || _lex.peek() == '.' || _lex.peek() == '_')) {
        if (_lex.peek() == '_') { _lex.advance(); continue; }
        if (_lex.peek() == '.') { if (hasDot) break; hasDot = true; }
        s += (char)_lex.peek(); _lex.advance();
    }
    // Scientific notation  e / E
    if (!_lex.atEnd() && (_lex.peek() == 'e' || _lex.peek() == 'E')) {
        s += (char)_lex.peek(); _lex.advance();
        if (!_lex.atEnd() && (_lex.peek() == '+' || _lex.peek() == '-'))
            { s += (char)_lex.peek(); _lex.advance(); }
        while (!_lex.atEnd() && std::isdigit(_lex.peek()))
            { s += (char)_lex.peek(); _lex.advance(); }
    }
    return {TokType::Num, s, ln};
}

Token Tokenizer::readName() {
    int ln = _lex.tokLine();
    std::string s;
    while (!_lex.atEnd() && (std::isalnum(_lex.peek()) || _lex.peek() == '_'))
        { s += (char)_lex.peek(); _lex.advance(); }

    auto it = kKeywords.find(s);
    if (it != kKeywords.end()) return {it->second, s, ln};
    if (kBuiltins.count(s)) return {TokType::Name, s, ln};
    return {TokType::Name, s, ln};
}

Token Tokenizer::readOp() {
    int ln = _lex.tokLine();
    char c = (char)_lex.peek();
    _lex.advance();
    int c2 = _lex.atEnd() ? 0 : _lex.peek();

    auto two = [&](TokType t, const char* txt) -> Token {
        _lex.advance();
        return {t, txt, ln};
    };

    if (c == '=' && c2 == '=') return two(TokType::Compare,    "==");
    if (c == '!' && c2 == '=') return two(TokType::Compare,    "!=");
    if (c == '<' && c2 == '=') return two(TokType::Compare,    "<=");
    if (c == '>' && c2 == '=') return two(TokType::Compare,    ">=");
    if (c == '<' && c2 == '<') return two(TokType::Op,         "<<");
    if (c == '>' && c2 == '>') return two(TokType::Op,         ">>");
    if (c == '*' && c2 == '*') return two(TokType::Op,         "**");
    if (c == '/' && c2 == '/') return two(TokType::Op,         "//");
    if (c == '%' && c2 == '/') return two(TokType::Op,         "%/");
    if (c == '+' && c2 == '=') return two(TokType::CompAssign, "+=");
    if (c == '-' && c2 == '=') return two(TokType::CompAssign, "-=");
    if (c == '*' && c2 == '=') return two(TokType::CompAssign, "*=");
    if (c == '/' && c2 == '=') return two(TokType::CompAssign, "/=");
    if (c == '%' && c2 == '=') return two(TokType::CompAssign, "%=");
    if (c == '^' && c2 == '=') return two(TokType::CompAssign, "^=");

    switch (c) {
        case '+': return {TokType::Op,      "+",                ln};
        case '-': return {TokType::Op,      "-",                ln};
        case '*': return {TokType::Op,      "*",                ln};
        case '/': return {TokType::Op,      "/",                ln};
        case '%': return {TokType::Op,      "%",                ln};
        case '^': return {TokType::Op,      "^",                ln};
        case '&': return {TokType::Op,      "&",                ln};
        case '|': return {TokType::Op,      "|",                ln};
        case '~': return {TokType::Op,      "~",                ln};
        case '<': return {TokType::Compare, "<",                ln};
        case '>': return {TokType::Compare, ">",                ln};
        case '=': return {TokType::Assign,  "=",                ln};
        default:  return {TokType::Err,     std::string(1, c),  ln};
    }
}
