#include "core/line_queue.h"

#include <chrono>

LineQueue::LineQueue(size_t capacity) : cap_(capacity ? capacity : 1) {}

void LineQueue::push(std::string line) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (closed_) return;
    if (q_.size() >= cap_) {
      q_.pop_front();
      dropped_++;
    }
    q_.push_back(std::move(line));
  }
  cv_.notify_one();
}

bool LineQueue::pop(std::string& out, int timeout_ms) {
  std::unique_lock<std::mutex> lk(mu_);
  cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&] { return closed_ || !q_.empty(); });
  if (q_.empty()) return false;
  out = std::move(q_.front());
  q_.pop_front();
  return true;
}

void LineQueue::close() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    closed_ = true;
  }
  cv_.notify_all();
}
