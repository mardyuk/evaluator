#include "lexer.hpp"
#include "tokenizer.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "vm.hpp"
#include "scope.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: nogo <file.nogo>\n";
        return 1;
    }
    try {
        std::string src = readFile(argv[1]);
        std::istringstream ss(src);
        Lexer     lx(ss);
        Tokenizer tok(lx);
        SymbolTable sym;
        Parser    par(tok, sym);
        auto root = par.parseProgram();
        Compiler  cmp(sym);
        ByteCode  bc = cmp.compile(root);
        VM vm;
        vm.load(bc);
        vm.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
