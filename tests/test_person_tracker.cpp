#include <vector>
#include "doctest.h"
#include "vision/person_tracker.h"

namespace {
PersonBox pb(float x, float y, float w = 30, float h = 100, float p = 0.9f) { return PersonBox{x, y, w, h, p}; }
}  // namespace

TEST_CASE("new detection is pending and not reported") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  CHECK(t.confirmed().empty());
  CHECK(t.needs_verify(0));
}

TEST_CASE("verify confirms when center inside 320 box even with low IoU") {
  PersonTracker t;
  t.update({pb(100, 50, 30, 100)}, 0);
  t.apply_verify({pb(90, 40, 60, 140)}, 0);  // IoU ~0.36 would pass too; center (115,100) is inside
  REQUIRE(t.confirmed().size() == 1);
  CHECK_FALSE(t.needs_verify(0));
}

TEST_CASE("verify confirms on center-inside alone when IoU < 0.3") {
  PersonTracker t;
  t.update({pb(100, 50, 30, 100)}, 0);
  t.apply_verify({pb(60, 20, 120, 220)}, 0);  // IoU = 3000/26400 ~ 0.11, center inside
  CHECK(t.confirmed().size() == 1);
}

TEST_CASE("verify rejects coat-like track and does not recheck before 5 s") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  t.apply_verify({}, 0);
  CHECK(t.confirmed().empty());
  for (int64_t ms = 1000; ms <= 4000; ms += 1000) t.update({pb(101, 50)}, ms);  // stays in view
  t.update({pb(101, 50)}, 4999);
  CHECK_FALSE(t.needs_verify(4999));
  CHECK(t.needs_verify(5000));
}

TEST_CASE("rejected track becomes confirmed on recheck") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  t.apply_verify({}, 0);
  t.update({pb(100, 50)}, 1000);
  t.update({pb(100, 50)}, 2000);
  t.update({pb(100, 50)}, 3000);
  t.update({pb(100, 50)}, 4000);
  t.update({pb(100, 50)}, 5000);
  REQUIRE(t.needs_verify(5000));
  t.apply_verify({pb(98, 48, 34, 104)}, 5000);
  CHECK(t.confirmed().size() == 1);
}

TEST_CASE("a 320 pass before the recheck time still confirms a rejected track it now sees") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  t.apply_verify({}, 0);
  t.update({pb(100, 50), pb(300, 50)}, 1000);            // a second, new person -> 320 pass
  CHECK(t.needs_verify(1000));
  t.apply_verify({pb(100, 50), pb(300, 50)}, 1000);      // every track is re-verified
  CHECK(t.confirmed().size() == 2);
}

TEST_CASE("ids stable across updates") {
  PersonTracker t;
  t.update({pb(100, 50), pb(250, 60)}, 0);
  t.apply_verify({pb(100, 50), pb(250, 60)}, 0);
  auto a = t.confirmed();
  REQUIRE(a.size() == 2);
  t.update({pb(255, 61), pb(103, 52)}, 300);
  auto b = t.confirmed();
  REQUIRE(b.size() == 2);
  for (const auto& x : a)
    for (const auto& y : b)
      if (x.id == y.id) CHECK(std::abs(x.box.x - y.box.x) < 10);
  CHECK(a[0].id != a[1].id);
}

TEST_CASE("clear drops all tracks") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  t.apply_verify({pb(100, 50)}, 0);
  t.clear();
  CHECK(t.confirmed().empty());
  CHECK_FALSE(t.needs_verify(0));
}

// ---- final-review fixes ----

TEST_CASE("confirmed track is rechecked every 5 s and drops when 320 no longer sees it") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  t.apply_verify({pb(100, 50)}, 0);
  REQUIRE(t.confirmed().size() == 1);
  for (int64_t ms = 1000; ms <= 4000; ms += 1000) {
    t.update({pb(100, 50)}, ms);
    CHECK_FALSE(t.needs_verify(ms));
  }
  t.update({pb(100, 50)}, 5000);
  CHECK(t.needs_verify(5000));
  t.apply_verify({}, 5000);  // the person left; only a coat is still matched by 160
  CHECK(t.confirmed().empty());
}

TEST_CASE("any 320 pass re-verifies every track") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  t.apply_verify({pb(100, 50)}, 0);        // confirmed (a person on the chair)
  t.update({pb(100, 50), pb(250, 50)}, 300);  // someone new appears -> a 320 pass runs
  t.apply_verify({pb(250, 50)}, 300);      // 320 sees only the newcomer
  auto c = t.confirmed();
  REQUIRE(c.size() == 1);
  CHECK(c[0].box.x == doctest::Approx(250));
}

TEST_CASE("slow person passes keep the same track") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  t.apply_verify({pb(100, 50)}, 0);
  const int id = t.confirmed()[0].id;
  t.update({pb(100, 50)}, 2000);  // e.g. fps_cap 2: passes 2 s apart
  t.update({pb(100, 50)}, 4000);
  CHECK_FALSE(t.needs_verify(4000));
  REQUIRE(t.confirmed().size() == 1);
  CHECK(t.confirmed()[0].id == id);
}

TEST_CASE("walking person keeps the id although IoU between passes is < 0.3") {
  PersonTracker t;
  t.update({pb(100, 50, 30, 100)}, 0);
  t.apply_verify({pb(100, 50, 30, 100)}, 0);
  const int id = t.confirmed()[0].id;
  t.update({pb(121, 52, 30, 100)}, 330);  // moved 0.7 box widths: IoU ~0.17
  CHECK_FALSE(t.needs_verify(330));
  REQUIRE(t.confirmed().size() == 1);
  CHECK(t.confirmed()[0].id == id);
}

TEST_CASE("a far-away box is a new track, not a jump") {
  PersonTracker t;
  t.update({pb(100, 50, 30, 100)}, 0);
  t.apply_verify({pb(100, 50, 30, 100)}, 0);
  t.update({pb(220, 50, 30, 100)}, 330);
  CHECK(t.needs_verify(330));
}

TEST_CASE("unseen track: reported for 1 missed pass, forgotten after 4") {
  PersonTracker t;
  t.update({pb(100, 50)}, 0);
  t.apply_verify({pb(100, 50)}, 0);
  t.update({}, 100);
  CHECK(t.confirmed().size() == 1);  // one 160 miss does not flicker
  t.update({}, 200);
  CHECK(t.confirmed().empty());      // two misses: not reported
  t.update({pb(100, 50)}, 300);
  CHECK(t.confirmed().size() == 1);  // came back within 4 passes: same confirmed track
  t.update({}, 400); t.update({}, 500); t.update({}, 600); t.update({}, 700);
  t.update({pb(100, 50)}, 800);
  CHECK(t.needs_verify(800));        // 4 misses: forgotten, new pending track
}
