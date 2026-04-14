#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include <algorithm>

// Tracks variable names, their storage locations, and scope nesting.
class SymbolTable {
public:
    // ─── Globals ─────────────────────────────────────────────────────────────
    bool   hasGlobal(const std::string& name) const {
        return _globals.count(name) > 0;
    }
    size_t globalSlotCount() const { return _nextGlobal; }
    const std::unordered_map<std::string, size_t>& globals() const { return _globals; }

    bool tryGlobalAddr(const std::string& name, size_t& out) const {
        auto it = _globals.find(name);
        if (it == _globals.end()) return false;
        out = it->second; return true;
    }
    size_t globalAddr(const std::string& name) {
        auto it = _globals.find(name);
        if (it != _globals.end()) return it->second;
        size_t a = _nextGlobal++;
        _globals[name] = a;
        return a;
    }

    // ─── Locals ──────────────────────────────────────────────────────────────
    bool tryLocal(const std::string& name, int32_t& off) const {
        for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
            auto f = it->vars.find(name);
            if (f != it->vars.end()) { off = f->second; return true; }
        }
        return false;
    }
    int32_t localOff(const std::string& name) {
        if (_scopes.empty()) throw std::runtime_error("no active scope");
        for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
            auto f = it->vars.find(name);
            if (f != it->vars.end()) return f->second;
        }
        Scope& cur = _scopes.back();
        int32_t off = cur.nextOff;
        cur.nextOff -= 4;
        cur.vars[name] = off;
        return off;
    }
    bool isLocal(const std::string& name) const {
        for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it)
            if (it->vars.count(name)) return true;
        return false;
    }
    bool hasInInnermost(const std::string& name) const {
        return !_scopes.empty() && _scopes.back().vars.count(name) > 0;
    }

    // ─── Enclosing-function lookup (nested fn support) ────────────────────
    bool tryEnclosing(const std::string& name, int32_t& off, int& hops) const {
        int hop = 1;
        for (auto si = _outerStack.rbegin(); si != _outerStack.rend(); ++si, ++hop)
            for (auto li = si->rbegin(); li != si->rend(); ++li) {
                auto f = li->vars.find(name);
                if (f != li->vars.end()) { off = f->second; hops = hop; return true; }
            }
        return false;
    }
    bool tryResolveLocal(const std::string& name, int32_t& off, int& hops) const {
        hops = 0;
        if (tryLocal(name, off)) return true;
        return tryEnclosing(name, off, hops);
    }

    // ─── Scope management ────────────────────────────────────────────────────
    void enterFunction() {
        _inFn = true;
        _outerStack.push_back(std::move(_scopes));
        _scopes.clear();
        _scopes.emplace_back(-4);
    }
    void exitFunction() {
        _inFn = false;
        _scopes.clear();
        if (!_outerStack.empty()) {
            _scopes = std::move(_outerStack.back());
            _outerStack.pop_back();
        }
    }
    void enterBlock() {
        int32_t start = _scopes.empty() ? -4 : _scopes.back().nextOff;
        _scopes.emplace_back(start);
    }
    void exitBlock() {
        if (_scopes.size() > 1) {
            int32_t used = _scopes.back().nextOff;
            _scopes.pop_back();
            if (!_scopes.empty() && used < _scopes.back().nextOff)
                _scopes.back().nextOff = used;
        }
    }

    bool   insideFunction() const { return _inFn; }
    bool   hasActiveScope()  const { return !_scopes.empty(); }
    size_t depth()           const { return _scopes.size(); }

    // ─── Program frame ───────────────────────────────────────────────────────
    void beginProgram() {
        _progSlots = 1;
        _outerStack.clear();
        _scopes.clear();
        _scopes.emplace_back(-4);
        _inFn = false;
    }
    void endProgram() {
        if (!_scopes.empty())
            _progSlots = std::max(_progSlots, frameSlotCount());
        _scopes.clear();
    }
    int programSlots() const { return _progSlots; }

    int32_t maxLocalOff() const {
        if (_scopes.empty()) return -4;
        int32_t m = -4;
        for (const auto& s : _scopes) m = std::min(m, s.nextOff);
        return m;
    }
    int frameSlotCount() const {
        if (_scopes.empty()) return 1;
        return std::max(1, (-maxLocalOff()) / 4 - 1);
    }

private:
    struct Scope {
        std::unordered_map<std::string, int32_t> vars;
        int32_t nextOff = -4;
        explicit Scope(int32_t s = -4) : nextOff(s) {}
    };

    std::unordered_map<std::string, size_t> _globals;
    size_t _nextGlobal = 0;

    std::vector<Scope> _scopes;
    bool _inFn = false;

    std::vector<std::vector<Scope>> _outerStack;
    int _progSlots = 1;
};
