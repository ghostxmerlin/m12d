#pragma once
#include <cstdint>
#include <deque>

// Event rate over a sliding window: (n-1) / (t_last - t_first) for events within
// window_ms of `now`. Unbiased, unlike counting events in the window.
class FpsMeter {
 public:
  explicit FpsMeter(int64_t window_ms) : window_ms_(window_ms) {}
  void tick(int64_t t_ms) { t_.push_back(t_ms); }
  float fps(int64_t now_ms) {
    while (!t_.empty() && now_ms - t_.front() > window_ms_) t_.pop_front();
    if (t_.size() < 2 || t_.back() == t_.front()) return 0.f;
    return (t_.size() - 1) * 1000.0f / static_cast<float>(t_.back() - t_.front());
  }

 private:
  int64_t window_ms_;
  std::deque<int64_t> t_;
};
