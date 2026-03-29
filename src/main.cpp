#include "lexer.hpp"
#include "parser.hpp"
#include "evaluator.hpp"
#include <iostream>
#include <iomanip>
#include <string>

static void evaluate(const std::string &input) {
    Lexer lexer(input);
    std::vector<Token> tokens = lexer.tokenize();

    Parser parser(std::move(tokens));
    ASTNodePtr root = parser.parse();

    Evaluator evaluator;
    double result = evaluator.evaluate(*root);

    std::cout << "= " << std::setprecision(10) << result << "\n";
}

int main() {
    std::cout << "Math Expression Evaluator\n";
    std::cout << "Operators: + - * / ( )\n";
    std::cout << "Type 'quit' or 'exit' to exit.\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "quit" || line == "exit") break;

        try {
            evaluate(line);
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }

    return 0;
}
