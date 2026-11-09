#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

// Bounded FIFO of text lines. When full, the oldest line is dropped.
class LineQueue {
 public:
  explicit LineQueue(size_t capacity);
  void push(std::string line);
  // Waits for a line. Returns false on timeout, or when closed and empty.
  bool pop(std::string& out, int timeout_ms);
  void close();
  uint64_t dropped() const { return dropped_.load(); }

 private:
  const size_t cap_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<std::string> q_;
  bool closed_ = false;
  std::atomic<uint64_t> dropped_{0};
};
