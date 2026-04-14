#pragma once
#include <variant>
#include <string>
#include <sstream>

// The three runtime types: number (double), string, or none (no value).
using Value = std::variant<std::monostate, double, std::string>;

inline bool isNone(const Value& v) { return std::holds_alternative<std::monostate>(v); }
inline bool isNum (const Value& v) { return std::holds_alternative<double>(v); }
inline bool isStr (const Value& v) { return std::holds_alternative<std::string>(v); }

inline double             asNum(const Value& v) { return std::get<double>(v); }
inline const std::string& asStr(const Value& v) { return std::get<std::string>(v); }

inline bool isFalsy(const Value& v) {
    if (isNone(v)) return true;
    if (isNum(v))  return asNum(v) == 0.0;
    if (isStr(v))  return asStr(v).empty();
    return true;
}
inline bool isTruthy(const Value& v) { return !isFalsy(v); }

inline std::string valStr(const Value& v) {
    if (isNone(v)) return "none";
    if (isStr(v))  return asStr(v);
    double d = asNum(v);
    if (d == static_cast<long long>(d))
        return std::to_string(static_cast<long long>(d));
    std::string s = std::to_string(d);
    s.erase(s.find_last_not_of('0') + 1);
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}
