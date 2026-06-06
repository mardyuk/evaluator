#pragma once
#include <string>

enum class TokType {
    Num,        // numeric literal
    Str,        // string literal
    Name,       // identifier / built-in function name
    Op,         // arithmetic / bitwise operator  (+ - * / ** // %/ % & | ^ ~ << >>)  comments: # and /* */
    Assign,     // =
    CompAssign, // += -= *= /= %= ^=
    Compare,    // == != < > <= >=
    LParen,     // (
    RParen,     // )
    LBrace,     // {
    RBrace,     // }
    LBracket,   // [
    RBracket,   // ]
    Semi,       // ;
    Comma,      // ,
    Question,   // ?
    Colon,      // :
    // keywords
    If,
    Else,
    While,
    For,
    Fn,         // fn
    Void,       // void
    Return,     // return
    Let,        // let  (variable declaration)
    Global,     // global
    Local,      // local
    Print,      // print
    Bool,       // true / false
    None,       // none
    Break,
    Continue,
    Switch,
    Case,
    Default,
    And,        // and
    Or,         // or
    Not,        // not
    MathConst,  // PI, E, INF, MAX
    Eof,
    Err,
};

struct Token {
    TokType     type;
    std::string text;
    int         line = 0;
};
