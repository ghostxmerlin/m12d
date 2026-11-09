#include <vector>
#include "doctest.h"
#include "vision/face_tracker.h"

namespace {
FaceBox box(float x, float y, float w = 40, float h = 50) { return FaceBox{x, y, w, h, 0.95f, {}}; }
GenderAgeResult ga(int gender, int age, float margin = 5.f) {
  GenderAgeResult r;
  r.gender = gender;
  r.age = age;
  r.margin = margin;
  return r;
}
}  // namespace

TEST_CASE("tracker keeps ids across frames by overlap") {
  FaceTracker t;
  auto a = t.update({box(100, 50), box(200, 60)}, 0);
  REQUIRE(a.size() == 2);
  CHECK(a[0] != a[1]);
  auto b = t.update({box(205, 62), box(103, 51)}, 50);  // order swapped, slightly moved
  CHECK(b[0] == a[1]);
  CHECK(b[1] == a[0]);
}

TEST_CASE("tracker forgets faces unseen for 1 s") {
  FaceTracker t;
  int id = t.update({box(100, 50)}, 0)[0];
  t.update({}, 500);
  CHECK(t.update({box(101, 50)}, 900)[0] == id);  // gap < 1 s: same person
  t.update({}, 1000);
  CHECK(t.update({box(101, 50)}, 2001)[0] != id);  // gap > 1 s: new person
}

TEST_CASE("new face gets 3 samples 200 ms apart, then refresh every 5 s") {
  FaceTracker t;
  auto ids = t.update({box(100, 50)}, 0);
  CHECK(t.pick_for_attr(ids, 0) == 0);
  t.add_sample(ids[0], ga(1, 40), 0);
  CHECK(t.pick_for_attr(ids, 100) == -1);  // too soon
  CHECK(t.pick_for_attr(ids, 200) == 0);
  t.add_sample(ids[0], ga(1, 42), 200);
  CHECK(t.pick_for_attr(ids, 400) == 0);
  t.add_sample(ids[0], ga(1, 44), 400);
  CHECK(t.pick_for_attr(ids, 600) == -1);   // 3 samples done
  CHECK(t.pick_for_attr(ids, 5399) == -1);
  CHECK(t.pick_for_attr(ids, 5400) == 0);   // refresh
}

TEST_CASE("pick prefers the face with the fewest samples") {
  FaceTracker t;
  auto ids = t.update({box(100, 50), box(220, 50)}, 0);
  t.add_sample(ids[0], ga(1, 40), 0);
  CHECK(t.pick_for_attr(ids, 300) == 1);
}

TEST_CASE("attributes: majority gender, unsure votes ignored, median age") {
  FaceTracker t;
  int id = t.update({box(100, 50)}, 0)[0];
  CHECK_FALSE(t.attr(id).have);
  t.add_sample(id, ga(1, 30), 0);
  t.add_sample(id, ga(0, 50, 0.4f), 200);  // unsure: no gender vote, age still counts
  t.add_sample(id, ga(1, 45), 400);
  FaceAttr a = t.attr(id);
  CHECK(a.have);
  CHECK(a.gender == 1);
  CHECK(a.age == 45);
  FaceTracker u;
  int id2 = u.update({box(100, 50)}, 0)[0];
  u.add_sample(id2, ga(0, 20, 0.3f), 0);
  CHECK(u.attr(id2).gender == -1);  // only unsure votes: unknown
}
