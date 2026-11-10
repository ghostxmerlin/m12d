#include <vector>
#include "doctest.h"
#include "vision/pairing.h"

namespace {
FaceBox fb(float cx, float cy) { return FaceBox{cx - 5, cy - 6, 10, 12, 0.95f, {}}; }
}  // namespace

TEST_CASE("face center in the top 40% of a person box pairs") {
  std::vector<PersonBox> p = {{100, 50, 40, 150, 0.9f}};  // top 40%: y 50..110
  CHECK(pair_faces({fb(120, 70)}, p) == std::vector<int>{0});
  CHECK(pair_faces({fb(120, 109)}, p) == std::vector<int>{0});
}

TEST_CASE("face center low in the box or outside does not pair") {
  std::vector<PersonBox> p = {{100, 50, 40, 150, 0.9f}};
  CHECK(pair_faces({fb(120, 150)}, p) == std::vector<int>{-1});  // lower part
  CHECK(pair_faces({fb(160, 70)}, p) == std::vector<int>{-1});   // right of the box
}

TEST_CASE("nested person boxes: the smaller one gets the face") {
  std::vector<PersonBox> p = {{50, 20, 200, 220, 0.9f}, {100, 50, 40, 150, 0.8f}};
  CHECK(pair_faces({fb(120, 70)}, p) == std::vector<int>{-1, 0});
}

TEST_CASE("each face pairs with at most one person, each person with at most one face") {
  std::vector<PersonBox> p = {{100, 50, 40, 150, 0.9f}};
  auto r = pair_faces({fb(115, 70), fb(125, 75)}, p);
  REQUIRE(r.size() == 1);
  CHECK((r[0] == 0 || r[0] == 1));
}
