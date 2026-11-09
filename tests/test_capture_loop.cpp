#include <atomic>
#include <chrono>
#include <thread>
#include "camera/capture_loop.h"
#include "doctest.h"

using namespace std::chrono;

namespace {
// Scripted camera: the first open_failures opens fail, then the first read_failures reads fail.
class FakeCamera : public ICamera {
 public:
  FakeCamera(int open_failures, int read_failures)
      : open_failures_(open_failures), read_failures_(read_failures) {}
  bool open() override {
    opens++;
    if (open_failures_ > 0) {
      open_failures_--;
      return false;
    }
    is_open = true;
    return true;
  }
  bool read(Frame& out, int /*timeout_ms*/) override {
    std::this_thread::sleep_for(milliseconds(2));
    if (!is_open) return false;
    if (read_failures_ > 0) {
      read_failures_--;
      return false;
    }
    out.yuyv.assign(8, 1);
    out.w = 2;
    out.h = 2;
    return true;
  }
  void close() override { is_open = false; }

  std::atomic<int> opens{0};
  std::atomic<bool> is_open{false};

 private:
  std::atomic<int> open_failures_, read_failures_;
};

struct Runner {
  ICamera& cam;
  LatestFrame slot;
  std::atomic<bool> stop{false};
  CaptureStats st;
  std::thread t;
  Runner(ICamera& c, int read_timeout_ms, int backoff_ms) : cam(c) {
    t = std::thread([this, read_timeout_ms, backoff_ms] {
      capture_loop(cam, slot, stop, st, read_timeout_ms, backoff_ms);
    });
  }
  void finish() {
    stop = true;
    t.join();
  }
};
}  // namespace

TEST_CASE("publishes frames") {
  FakeCamera cam(0, 0);
  Runner r(cam, 1000, 500);
  std::this_thread::sleep_for(milliseconds(50));
  r.finish();
  CHECK(r.st.frames > 0);
  Frame out;
  CHECK(r.slot.wait_newer(0, out, 10));
  CHECK(out.w == 2);
  CHECK_FALSE(cam.is_open);  // closed on exit
}

TEST_CASE("reopens after consecutive read failures") {
  FakeCamera cam(0, 3);
  Runner r(cam, 1000, 10);
  std::this_thread::sleep_for(milliseconds(150));
  r.finish();
  CHECK(r.st.errors >= 3);
  CHECK(r.st.reopens >= 1);
  CHECK(cam.opens >= 2);
  CHECK(r.st.frames > 0);
}

TEST_CASE("two read failures do not reopen") {
  FakeCamera cam(0, 2);
  Runner r(cam, 1000, 10);
  std::this_thread::sleep_for(milliseconds(100));
  r.finish();
  CHECK(r.st.errors == 2);
  CHECK(r.st.reopens == 0);
  CHECK(cam.opens == 1);
}

TEST_CASE("open failure retried with backoff") {
  FakeCamera cam(2, 0);
  auto t0 = steady_clock::now();
  Runner r(cam, 1000, 10);
  Frame out;
  bool got = r.slot.wait_newer(0, out, 500);
  auto ms = duration_cast<milliseconds>(steady_clock::now() - t0).count();
  r.finish();
  CHECK(got);
  CHECK(cam.opens == 3);
  CHECK(ms >= 20);  // two backoffs of 10ms
  CHECK(ms < 500);
}

TEST_CASE("stop exits within one timeout") {
  FakeCamera cam(1000000, 0);  // never opens: loop sits in backoff
  Runner r(cam, 200, 5000);
  std::this_thread::sleep_for(milliseconds(20));
  auto t0 = steady_clock::now();
  r.finish();
  CHECK(duration_cast<milliseconds>(steady_clock::now() - t0).count() < 300);
}
