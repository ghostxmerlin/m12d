#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

struct Frame {
  std::vector<uint8_t> yuyv;
  int w = 0, h = 0;
  uint64_t id = 0;
  int64_t ts_ms = 0;
};

// Single-slot "latest frame" buffer. Publishers overwrite; readers copy the newest.
class LatestFrame {
 public:
  // Assigns id = previous id + 1, then wakes all waiters.
  void publish(Frame&& f);
  // Copies the newest frame into out if its id > last_id. Returns false on timeout or after stop().
  bool wait_newer(uint64_t last_id, Frame& out, int timeout_ms);
  // Wakes all waiters; afterwards wait_newer returns false immediately.
  void stop();

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  Frame cur_;
  uint64_t next_id_ = 1;
  bool stopped_ = false;
};
