#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "core/line_queue.h"
#include "io/line_splitter.h"

using ChanId = int;  // 0 = serial, 1.. = TCP clients in accept order

struct IoStats {
  std::atomic<uint64_t> dropped{0};           // lines dropped: per-channel buffer full or push queue overflow
  std::atomic<uint64_t> rejected_clients{0};  // TCP connections refused because max_clients was reached
  std::atomic<uint64_t> clients{0};           // TCP clients currently connected
};

// One poll() loop serving the serial port and TCP clients with the same line protocol.
// Takes ownership of serial_fd and listen_fd (either may be -1).
class IoLoop {
 public:
  // Runs on the IO thread. Returns the reply line (without '\n'); empty = no reply.
  using Handler = std::function<std::string(ChanId, std::string_view line, bool too_long)>;
  static constexpr size_t kChanBufBytes = 8192;
  static constexpr size_t kPushQueueLines = 64;
  static constexpr int kClientSndBuf = 16384;
  static constexpr size_t kReadBudget = 4096;  // max input bytes per channel per poll round

  IoLoop(int serial_fd, int listen_fd, size_t max_clients, Handler h);
  ~IoLoop();
  IoLoop(const IoLoop&) = delete;
  IoLoop& operator=(const IoLoop&) = delete;

  void run(std::atomic<bool>& stop);
  void push(std::string line);              // any thread
  void set_subscribed(ChanId c, bool on);   // IO thread (inside handler) only
  bool subscribed(ChanId c) const;          // IO thread (inside handler) only
  const IoStats& stats() const { return stats_; }

 private:
  struct Chan {
    ChanId id;
    int fd;
    bool is_serial;
    bool subscribed = true;
    bool dead = false;
    LineSplitter splitter;
    std::string out;
  };
  Chan* find(ChanId c);
  const Chan* find(ChanId c) const;
  void enqueue(Chan& ch, const std::string& line);
  void flush(Chan& ch);
  void read_input(Chan& ch);
  void accept_clients();
  void drain_push_queue();
  void close_chan(Chan& ch);

  int listen_fd_;
  int wake_fd_;
  size_t max_clients_;
  Handler handler_;
  std::vector<Chan> chans_;
  ChanId next_id_ = 1;
  LineQueue queue_{kPushQueueLines};
  uint64_t queue_drops_seen_ = 0;
  IoStats stats_;
};
