#include "doctest.h"
#include "sys/sysinfo.h"
#include "sys/thermal.h"

TEST_CASE("parse") {
  CHECK(parse_temp_r("Temperature 74\n") == 74);
  CHECK(parse_temp_r("Temperature 74") == 74);
  CHECK_FALSE(parse_temp_r(""));
  CHECK_FALSE(parse_temp_r("Temp=74"));
  CHECK_FALSE(parse_temp_r("Temperature abc"));
  CHECK_FALSE(parse_temp_r("Temperature 74x"));
  CHECK_FALSE(parse_temp_r("Temperature "));
}

TEST_CASE("pauses at 90") {
  ThermalGuard g;
  CHECK_FALSE(g.update(89, 0));
  CHECK(g.update(90, 1000));
  CHECK(g.last_temp() == 90);
}

TEST_CASE("hysteresis") {
  ThermalGuard g;
  CHECK(g.update(90, 0));
  CHECK(g.update(88, 1000));
  CHECK(g.update(87, 2000));
  CHECK_FALSE(g.update(86, 3000));
  CHECK_FALSE(g.update(89, 4000));
}

TEST_CASE("unreadable fails safe") {
  ThermalGuard g;
  CHECK(g.last_temp() == -1);
  CHECK_FALSE(g.update(std::nullopt, 0));
  CHECK_FALSE(g.update(std::nullopt, 2999));
  CHECK(g.update(std::nullopt, 3000));
  CHECK(g.update(std::nullopt, 5000));
  CHECK_FALSE(g.update(70, 5001));
  // a valid reading resets the unreadable clock
  CHECK_FALSE(g.update(std::nullopt, 7000));
  CHECK(g.update(std::nullopt, 8001));
}

TEST_CASE("unreadable while hot stays paused after readings return hot") {
  ThermalGuard g;
  CHECK(g.update(91, 0));
  CHECK(g.update(std::nullopt, 1000));
  CHECK(g.update(88, 2000));  // still above resume threshold
  CHECK_FALSE(g.update(80, 3000));
}

TEST_CASE("sysinfo parsers") {
  CHECK(parse_mem_available_kb("MemTotal:  114000 kB\nMemFree: 1 kB\nMemAvailable:   75123 kB\n") == 75123);
  CHECK(parse_mem_available_kb("MemTotal: 1 kB\n") == -1);
  CHECK(parse_uptime_s("12345.67 2345.00\n") == 12345);
  CHECK(parse_uptime_s("") == -1);
  CHECK(parse_cpu_mhz("1400000\n") == doctest::Approx(1400.0f));
  CHECK(parse_cpu_mhz("x") == doctest::Approx(0.0f));
}
