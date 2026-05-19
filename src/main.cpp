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
#include <vector>
#include <filesystem>
#include <set>
#include <stdexcept>

// ─── Import preprocessing ─────────────────────────────────────────────────────

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string preprocess(const std::string& src, const std::string& baseDir,
                               std::set<std::string>& seen) {
    std::istringstream in(src);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        // look for: import "path"
        size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (line.substr(i, 6) == "import" && i + 6 < line.size() && line[i + 6] == ' ') {
            size_t q1 = line.find('"', i + 7);
            size_t q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string relPath = line.substr(q1 + 1, q2 - q1 - 1);
                std::string absPath = baseDir.empty() ? relPath : baseDir + "/" + relPath;
                if (!seen.count(absPath)) {
                    seen.insert(absPath);
                    std::string importSrc = readFile(absPath);
                    std::string importDir = std::filesystem::path(absPath).parent_path().string();
                    out << preprocess(importSrc, importDir, seen) << "\n";
                }
                continue;
            }
        }
        out << line << "\n";
    }
    return out.str();
}

// ─── Pipeline ─────────────────────────────────────────────────────────────────

static std::string loadAndPreprocess(const std::string& path) {
    std::string src = readFile(path);
    std::string dir = std::filesystem::path(path).parent_path().string();
    std::set<std::string> seen;
    seen.insert(std::filesystem::absolute(path).string());
    return preprocess(src, dir, seen);
}

static ByteCode compileSource(const std::string& src) {
    std::istringstream ss(src);
    Lexer     lx(ss);
    Tokenizer tok(lx);
    SymbolTable sym;
    Parser    par(tok, sym);
    auto root = par.parseProgram();
    Compiler  cmp(sym);
    return cmp.compile(root);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

static void usage(const char* argv0) {
    std::cerr << "usage:\n"
              << "  " << argv0 << " <file.nogo>              run source file\n"
              << "  " << argv0 << " compile <in> [out.ngb]   compile to bytecode\n"
              << "  " << argv0 << " run <file.ngb> [--debug] execute bytecode\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    std::string cmd = argv[1];

    try {
        // ── compile mode ──────────────────────────────────────────────────────
        if (cmd == "compile") {
            if (argc < 3) { usage(argv[0]); return 1; }
            std::string inPath  = argv[2];
            std::string outPath = (argc >= 4) ? argv[3] : (inPath.substr(0, inPath.rfind('.')) + ".ngb");

            std::string src = loadAndPreprocess(inPath);
            ByteCode bc = compileSource(src);
            writeBytecode(bc, outPath);
            std::cout << "compiled: " << outPath << "\n";
            return 0;
        }

        // ── run mode (pre-compiled bytecode) ──────────────────────────────────
        if (cmd == "run") {
            if (argc < 3) { usage(argv[0]); return 1; }
            std::string inPath = argv[2];
            bool dbg = false;
            for (int k = 3; k < argc; ++k) if (std::string(argv[k]) == "--debug") dbg = true;

            VM vm(dbg);
            vm.loadFile(inPath);
            vm.run();
            return 0;
        }

        // ── direct source execution ───────────────────────────────────────────
        std::string src = loadAndPreprocess(cmd);
        ByteCode bc = compileSource(src);
        VM vm(false);
        vm.load(bc);
        vm.run();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
