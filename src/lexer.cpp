#include "lexer.hpp"

Lexer::Lexer(std::istream& src) : _src(src), _cur(src.get()) {}

void Lexer::advance() {
    if (_cur == '\n') _line++;
    _cur = _src.get();
}

int Lexer::peekNext() const {
    // std::istream::peek() reads the next char without consuming it
    return _src.peek();
}
