// SPDX-License-Identifier: GPL-3.0-or-later
// A small JSON reader, enough for a glTF scene description.
#pragma once
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace mini_json {
struct Value {
  enum class Type { null, boolean, number, string, array, object } type = Type::null;
  bool b = false; double n = 0; std::string s;
  std::vector<Value> a; std::map<std::string, Value> o;
  bool has(const std::string &k) const { return type == Type::object && o.count(k); }
  const Value &operator[](const std::string &k) const { auto it = o.find(k); if (type != Type::object || it == o.end()) throw std::runtime_error("JSON key missing: " + k); return it->second; }
  const Value &operator[](std::size_t i) const { if (type != Type::array || i >= a.size()) throw std::runtime_error("JSON index out of range"); return a[i]; }
  std::size_t size() const { return type == Type::array ? a.size() : 0; }
  double num() const { if (type != Type::number) throw std::runtime_error("JSON number expected"); return n; }
  const std::string &str() const { if (type != Type::string) throw std::runtime_error("JSON string expected"); return s; }
};

class Parser {
public:
  explicit Parser(const std::string &text) : t(text) {}
  Value parse() { Value v = value(); ws(); if (i != t.size()) throw std::runtime_error("JSON trailing data"); return v; }
private:
  const std::string &t; std::size_t i = 0; int depth = 0;
  void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) ++i; }
  char peek() { ws(); if (i >= t.size()) throw std::runtime_error("JSON ended early"); return t[i]; }
  void expect(char c) { if (peek() != c) throw std::runtime_error(std::string("JSON expected ") + c); ++i; }
  std::string string()
  {
    expect('"'); std::string out;
    while (i < t.size() && t[i] != '"') {
      char c = t[i++];
      if (c == '\\') {
        if (i >= t.size()) break;
        char e = t[i++];
        switch (e) {
        case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
        case 'b': out += '\b'; break; case 'f': out += '\f'; break;
        case 'u': { if (i + 4 > t.size()) throw std::runtime_error("JSON bad escape"); unsigned cp = std::strtoul(t.substr(i, 4).c_str(), nullptr, 16); i += 4; out += cp < 128 ? char(cp) : '?'; break; }
        default: out += e;
        }
      } else out += c;
    }
    if (i >= t.size()) throw std::runtime_error("JSON unterminated string");
    ++i; return out;
  }
  Value value()
  {
    if (++depth > 64) throw std::runtime_error("JSON nested too deeply");
    Value v; char c = peek();
    if (c == '{') {
      ++i; v.type = Value::Type::object;
      if (peek() == '}') ++i;
      else for (;;) { std::string k = string(); expect(':'); v.o[k] = value(); if (peek() == ',') { ++i; continue; } expect('}'); break; }
    } else if (c == '[') {
      ++i; v.type = Value::Type::array;
      if (peek() == ']') ++i;
      else for (;;) { v.a.push_back(value()); if (peek() == ',') { ++i; continue; } expect(']'); break; }
    } else if (c == '"') { v.type = Value::Type::string; v.s = string(); }
    else if (!t.compare(i, 4, "true")) { v.type = Value::Type::boolean; v.b = true; i += 4; }
    else if (!t.compare(i, 5, "false")) { v.type = Value::Type::boolean; i += 5; }
    else if (!t.compare(i, 4, "null")) { i += 4; }
    else {
      const char *start = t.c_str() + i; char *end = nullptr;
      v.n = std::strtod(start, &end);
      if (end == start) throw std::runtime_error("JSON value expected");
      v.type = Value::Type::number; i += end - start;
    }
    --depth; return v;
  }
};
inline Value parse(const std::string &text) { return Parser(text).parse(); }
}
