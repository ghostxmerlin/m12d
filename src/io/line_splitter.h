#pragma once
#include <functional>
#include <string>
#include <string_view>

// Splits a byte stream into lines. Strips a trailing '\r'; ignores empty lines.
// A line longer than max_len is discarded up to the next '\n' and reported once
// via on_line("", too_long=true).
class LineSplitter {
 public:
  explicit LineSplitter(size_t max_len = 512) : max_len_(max_len) {}
  void feed(const char* p, size_t n, const std::function<void(std::string_view line, bool too_long)>& on_line);

 private:
  size_t max_len_;
  std::string buf_;
  bool discarding_ = false;
};
