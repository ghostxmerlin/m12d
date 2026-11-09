#include "sys/thermal.h"

#include <cctype>
#include <fstream>
#include <sstream>

std::optional<int> parse_temp_r(const std::string& s) {
  const std::string prefix = "Temperature ";
  if (s.compare(0, prefix.size(), prefix) != 0) return std::nullopt;
  size_t i = prefix.size();
  size_t start = i;
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
  if (i == start || i - start > 4) return std::nullopt;
  const int v = std::stoi(s.substr(start, i - start));
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
  if (i != s.size()) return std::nullopt;
  return v;
}

std::optional<int> read_temp_c(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::nullopt;
  std::stringstream ss;
  ss << f.rdbuf();
  return parse_temp_r(ss.str());
}

ThermalGuard::ThermalGuard(int pause_at, int resume_below, int unreadable_ms)
    : pause_at_(pause_at), resume_below_(resume_below), unreadable_ms_(unreadable_ms) {}

bool ThermalGuard::update(std::optional<int> temp, int64_t now_ms) {
  if (!have_ref_) {
    have_ref_ = true;
    last_valid_ms_ = now_ms;
  }
  if (temp) {
    last_temp_ = *temp;
    last_valid_ms_ = now_ms;
    unreadable_ = false;
    if (*temp >= pause_at_)
      hot_ = true;
    else if (*temp < resume_below_)
      hot_ = false;
  } else if (now_ms - last_valid_ms_ >= unreadable_ms_) {
    unreadable_ = true;
  }
  return hot_ || unreadable_;
}
