#pragma once
#include "lexer.hpp"
#include "token.hpp"

// Groups the raw character stream into tokens for the parser.
class Tokenizer {
public:
    explicit Tokenizer(Lexer& lex);
    Token next();

private:
    Lexer& _lex;

    void  skip();          // consume whitespace and comments
    Token readStr(char q); // string literal after opening quote
    Token readNum();       // numeric literal
    Token readName();      // identifier or keyword
    Token readOp();        // operator or punctuation
};
