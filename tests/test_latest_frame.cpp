#include <chrono>
#include <thread>
#include "core/latest_frame.h"
#include "doctest.h"

using namespace std::chrono;

static Frame make_frame(uint8_t v) {
  Frame f;
  f.yuyv.assign(8, v);
  f.w = 2;
  f.h = 2;
  return f;
}

TEST_CASE("wait_newer returns published frame") {
  LatestFrame slot;
  slot.publish(make_frame(7));
  Frame out;
  CHECK(slot.wait_newer(0, out, 100));
  CHECK(out.id == 1);
  CHECK(out.yuyv[0] == 7);
}

TEST_CASE("wait_newer times out when nothing newer") {
  LatestFrame slot;
  slot.publish(make_frame(1));
  Frame out;
  auto t0 = steady_clock::now();
  CHECK_FALSE(slot.wait_newer(1, out, 50));
  CHECK(duration_cast<milliseconds>(steady_clock::now() - t0).count() >= 45);
}

TEST_CASE("stop wakes waiter") {
  LatestFrame slot;
  bool result = true;
  long waited_ms = 0;
  std::thread t([&] {
    Frame out;
    auto t0 = steady_clock::now();
    result = slot.wait_newer(0, out, 5000);
    waited_ms = duration_cast<milliseconds>(steady_clock::now() - t0).count();
  });
  std::this_thread::sleep_for(milliseconds(10));
  slot.stop();
  t.join();
  CHECK_FALSE(result);
  CHECK(waited_ms < 200);
  Frame out;
  CHECK_FALSE(slot.wait_newer(0, out, 1000));  // returns immediately after stop
}

TEST_CASE("skips to latest") {
  LatestFrame slot;
  slot.publish(make_frame(1));
  slot.publish(make_frame(2));
  slot.publish(make_frame(3));
  Frame out;
  CHECK(slot.wait_newer(0, out, 100));
  CHECK(out.id == 3);
  CHECK(out.yuyv[0] == 3);
}
