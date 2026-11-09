#include <chrono>
#include <thread>
#include "core/line_queue.h"
#include "doctest.h"

using namespace std::chrono;

TEST_CASE("drop oldest when full") {
  LineQueue q(2);
  q.push("a");
  q.push("b");
  q.push("c");
  std::string out;
  CHECK(q.pop(out, 10));
  CHECK(out == "b");
  CHECK(q.pop(out, 10));
  CHECK(out == "c");
  CHECK(q.dropped() == 1);
}

TEST_CASE("pop times out on empty") {
  LineQueue q(4);
  std::string out;
  auto t0 = steady_clock::now();
  CHECK_FALSE(q.pop(out, 30));
  CHECK(duration_cast<milliseconds>(steady_clock::now() - t0).count() >= 25);
}

TEST_CASE("close unblocks pop") {
  LineQueue q(4);
  bool result = true;
  long waited_ms = 0;
  std::thread t([&] {
    std::string out;
    auto t0 = steady_clock::now();
    result = q.pop(out, 5000);
    waited_ms = duration_cast<milliseconds>(steady_clock::now() - t0).count();
  });
  std::this_thread::sleep_for(milliseconds(10));
  q.close();
  t.join();
  CHECK_FALSE(result);
  CHECK(waited_ms < 200);
}
