#include "io/line_splitter.h"

void LineSplitter::feed(const char* p, size_t n,
                        const std::function<void(std::string_view line, bool too_long)>& on_line) {
  for (size_t i = 0; i < n; i++) {
    const char c = p[i];
    if (c == '\n') {
      if (!discarding_) {
        if (!buf_.empty() && buf_.back() == '\r') buf_.pop_back();
        if (!buf_.empty()) on_line(buf_, false);
      }
      discarding_ = false;
      buf_.clear();
      continue;
    }
    if (discarding_) continue;
    buf_.push_back(c);
    // A trailing '\r' does not count against the limit (it is stripped).
    const size_t len = buf_.size() - (buf_.back() == '\r' ? 1 : 0);
    if (len > max_len_) {
      on_line(std::string_view(), true);
      discarding_ = true;
      buf_.clear();
    }
  }
}
