#include "lexer.hpp"
#include "parser.hpp"
#include "evaluator.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <regex>

// Returns true if the line is an assignment like "x = <expr>", sets the variable.
static bool tryAssign(const std::string &input, Evaluator &evaluator) {
    static const std::regex assignRe(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$)");
    std::smatch m;
    if (!std::regex_match(input, m, assignRe)) return false;

    const std::string name = m[1];
    const std::string rhs  = m[2];

    Lexer lexer(rhs);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto root = parser.parse();
    double value = evaluator.evaluate(*root);
    evaluator.setVariable(name, value);

    std::cout << name << " = " << std::setprecision(10) << value << "\n";
    return true;
}

int main() {
    std::cout << "Math Expression Evaluator\n";
    std::cout << "Operators : + - * / ( )\n";
    std::cout << "Variables : assign with  x = 3.14  then use in expressions\n";
    std::cout << "Type 'quit' or 'exit' to exit.\n";

    Evaluator evaluator;
    std::string line;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "quit" || line == "exit") break;

        try {
            if (tryAssign(line, evaluator)) continue;

            Lexer lexer(line);
            auto tokens = lexer.tokenize();
            Parser parser(std::move(tokens));
            auto root = parser.parse();
            double result = evaluator.evaluate(*root);
            std::cout << "= " << std::setprecision(10) << result << "\n";
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }

    return 0;
}
