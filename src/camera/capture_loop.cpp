#include "camera/capture_loop.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace {
void sleep_unless_stopped(int ms, const std::atomic<bool>& stop) {
  using namespace std::chrono;
  const auto end = steady_clock::now() + milliseconds(ms);
  while (!stop) {
    const auto left = duration_cast<milliseconds>(end - steady_clock::now()).count();
    if (left <= 0) break;
    std::this_thread::sleep_for(milliseconds(std::min<long long>(left, 20)));
  }
}
}  // namespace

void capture_loop(ICamera& cam, LatestFrame& slot, std::atomic<bool>& stop, CaptureStats& st,
                  int read_timeout_ms, int reopen_backoff_ms) {
  constexpr int kFailuresBeforeReopen = 3;
  bool opened = false;
  int consecutive_failures = 0;
  while (!stop) {
    if (!opened) {
      if (cam.open()) {
        opened = true;
        consecutive_failures = 0;
      } else {
        sleep_unless_stopped(reopen_backoff_ms, stop);
        continue;
      }
    }
    Frame f;
    if (cam.read(f, read_timeout_ms)) {
      consecutive_failures = 0;
      st.frames++;
      slot.publish(std::move(f));
      continue;
    }
    st.errors++;
    if (++consecutive_failures >= kFailuresBeforeReopen) {
      cam.close();
      opened = false;
      st.reopens++;
      sleep_unless_stopped(reopen_backoff_ms, stop);
    }
  }
  if (opened) cam.close();
}
