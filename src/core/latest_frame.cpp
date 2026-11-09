#include "core/latest_frame.h"

#include <chrono>

void LatestFrame::publish(Frame&& f) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    f.id = next_id_++;
    cur_ = std::move(f);
  }
  cv_.notify_all();
}

bool LatestFrame::wait_newer(uint64_t last_id, Frame& out, int timeout_ms) {
  std::unique_lock<std::mutex> lk(mu_);
  bool ready = cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                            [&] { return stopped_ || cur_.id > last_id; });
  if (!ready || stopped_) return false;
  out = cur_;
  return true;
}

void LatestFrame::stop() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    stopped_ = true;
  }
  cv_.notify_all();
}
