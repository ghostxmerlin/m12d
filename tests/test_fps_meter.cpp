#include "core/fps_meter.h"
#include "doctest.h"

TEST_CASE("fps from evenly spaced events") {
  FpsMeter m(2000);
  for (int i = 0; i <= 20; i++) m.tick(i * 140);  // 7.14 fps
  CHECK(m.fps(20 * 140) == doctest::Approx(1000.0 / 140).epsilon(0.001));
}

TEST_CASE("fewer than two events in window is zero") {
  FpsMeter m(2000);
  CHECK(m.fps(0) == 0);
  m.tick(100);
  CHECK(m.fps(100) == 0);
}

TEST_CASE("old events expire") {
  FpsMeter m(2000);
  for (int i = 0; i < 10; i++) m.tick(i * 100);
  CHECK(m.fps(900) == doctest::Approx(10.0));
  CHECK(m.fps(5000) == 0);  // nothing in the last 2 s: detection stopped
}
