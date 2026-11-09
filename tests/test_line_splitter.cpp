#include <string>
#include <vector>
#include "doctest.h"
#include "io/line_splitter.h"

namespace {
struct Collect {
  std::vector<std::string> lines;
  int too_long = 0;
  void operator()(std::string_view l, bool tl) {
    if (tl)
      too_long++;
    else
      lines.emplace_back(l);
  }
};
void feed(LineSplitter& s, const std::string& d, Collect& c) {
  s.feed(d.data(), d.size(), [&](std::string_view l, bool tl) { c(l, tl); });
}
}  // namespace

TEST_CASE("splits across chunks") {
  LineSplitter s;
  Collect c;
  feed(s, "ab", c);
  feed(s, "c\nde", c);
  feed(s, "f\n", c);
  REQUIRE(c.lines.size() == 2);
  CHECK(c.lines[0] == "abc");
  CHECK(c.lines[1] == "def");
}

TEST_CASE("strips CR") {
  LineSplitter s;
  Collect c;
  feed(s, "x\r\n", c);
  REQUIRE(c.lines.size() == 1);
  CHECK(c.lines[0] == "x");
}

TEST_CASE("too long discarded once") {
  LineSplitter s(8);
  Collect c;
  feed(s, std::string(20, 'a') + "\n", c);
  feed(s, "ok\n", c);
  CHECK(c.too_long == 1);
  REQUIRE(c.lines.size() == 1);
  CHECK(c.lines[0] == "ok");
}

TEST_CASE("exactly max_len is accepted") {
  LineSplitter s(8);
  Collect c;
  feed(s, "12345678\n", c);
  CHECK(c.too_long == 0);
  REQUIRE(c.lines.size() == 1);
  CHECK(c.lines[0] == "12345678");
}

TEST_CASE("empty lines ignored") {
  LineSplitter s;
  Collect c;
  feed(s, "\n\n\r\n", c);
  CHECK(c.lines.empty());
  CHECK(c.too_long == 0);
}
