#pragma once
#include <istream>

// Character-level stream with line tracking.
class Lexer {
public:
    explicit Lexer(std::istream& src);

    void advance();
    int  peek()     const { return _cur; }
    int  peekNext() const;   // look at the char after current without consuming
    bool atEnd()    const { return _cur == EOF; }

    int  curLine()  const { return _line; }
    void markStart()      { _tokLine = _line; }
    int  tokLine()  const { return _tokLine; }

private:
    std::istream& _src;
    int _cur     = 0;
    int _line    = 1;
    int _tokLine = 1;
};
