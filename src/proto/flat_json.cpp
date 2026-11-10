#include "proto/flat_json.h"

#include <cstdio>

#include <cstdlib>

namespace {

class Parser {
 public:
  explicit Parser(std::string_view s) : s_(s) {}

  std::optional<std::map<std::string, FlatValue>> object() {
    std::map<std::string, FlatValue> out;
    ws();
    if (!eat('{')) return std::nullopt;
    ws();
    if (eat('}')) return end(out);
    for (;;) {
      std::string key;
      FlatValue v;
      ws();
      if (!string(key)) return std::nullopt;
      ws();
      if (!eat(':')) return std::nullopt;
      ws();
      if (!value(v)) return std::nullopt;
      if (out.count(key) || out.size() >= 32) return std::nullopt;
      out.emplace(std::move(key), std::move(v));
      ws();
      if (eat('}')) return end(out);
      if (!eat(',')) return std::nullopt;
    }
  }

 private:
  std::optional<std::map<std::string, FlatValue>> end(std::map<std::string, FlatValue>& out) {
    ws();
    if (i_ != s_.size()) return std::nullopt;
    return std::move(out);
  }
  void ws() {
    while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\r' || s_[i_] == '\n')) i_++;
  }
  bool eat(char c) {
    if (i_ < s_.size() && s_[i_] == c) {
      i_++;
      return true;
    }
    return false;
  }
  bool lit(std::string_view w) {
    if (s_.substr(i_, w.size()) != w) return false;
    i_ += w.size();
    return true;
  }
  bool value(FlatValue& v) {
    if (i_ >= s_.size()) return false;
    const char c = s_[i_];
    if (c == '"') {
      std::string str;
      if (!string(str)) return false;
      v = std::move(str);
      return true;
    }
    if (lit("true")) return v = true, true;
    if (lit("false")) return v = false, true;
    if (lit("null")) return v = nullptr, true;
    double d;
    if (!number(d)) return false;
    v = d;
    return true;
  }
  static bool digit(char c) { return c >= '0' && c <= '9'; }
  bool number(double& d) {
    const size_t start = i_;
    eat('-');
    if (i_ >= s_.size()) return false;
    if (s_[i_] == '0') {
      i_++;
    } else if (digit(s_[i_])) {
      while (i_ < s_.size() && digit(s_[i_])) i_++;
    } else {
      return false;
    }
    if (eat('.')) {
      if (i_ >= s_.size() || !digit(s_[i_])) return false;
      while (i_ < s_.size() && digit(s_[i_])) i_++;
    }
    if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
      i_++;
      if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) i_++;
      if (i_ >= s_.size() || !digit(s_[i_])) return false;
      while (i_ < s_.size() && digit(s_[i_])) i_++;
    }
    if (i_ < s_.size() && digit(s_[i_])) return false;  // e.g. "01"
    d = std::strtod(std::string(s_.substr(start, i_ - start)).c_str(), nullptr);
    return true;
  }
  int hex4() {
    if (i_ + 4 > s_.size()) return -1;
    int v = 0;
    for (int k = 0; k < 4; k++) {
      const char c = s_[i_++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= c - '0';
      else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
      else return -1;
    }
    return v;
  }
  static void utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    } else if (cp < 0x800) {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += static_cast<char>(0xE0 | (cp >> 12));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (cp >> 18));
      out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }
  bool string(std::string& out) {
    if (!eat('"')) return false;
    while (i_ < s_.size()) {
      const unsigned char c = static_cast<unsigned char>(s_[i_++]);
      if (c == '"') return true;
      if (c < 0x20) return false;
      if (c != '\\') {
        out += static_cast<char>(c);
        continue;
      }
      if (i_ >= s_.size()) return false;
      switch (s_[i_++]) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          int cp = hex4();
          if (cp < 0) return false;
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (!lit("\\u")) return false;
            int lo = hex4();
            if (lo < 0xDC00 || lo > 0xDFFF) return false;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return false;
          }
          utf8(out, static_cast<unsigned>(cp));
          break;
        }
        default: return false;
      }
    }
    return false;
  }

  std::string_view s_;
  size_t i_ = 0;
};

}  // namespace

std::optional<std::map<std::string, FlatValue>> parse_flat_json(std::string_view s) { return Parser(s).object(); }

std::string json_str(const std::string& s) {
  std::string o = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      o += '\\';
      o += static_cast<char>(c);
    } else if (c < 0x20) {
      char b[8];
      std::snprintf(b, sizeof b, "\\u%04x", c);
      o += b;
    } else {
      o += static_cast<char>(c);
    }
  }
  return o + "\"";
}
