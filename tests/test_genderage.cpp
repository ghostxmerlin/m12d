#include <vector>
#include "doctest.h"
#include "vision/genderage.h"

TEST_CASE("genderage crop: box center to (48,48), long side * 1.5 -> 96 px") {
  FaceBox b{100, 50, 40, 60, 0.9f, {}};  // center (120, 80), long side 60 -> scale 96/90
  float tm[6];
  genderage_crop_transform(b, tm);
  const float s = 96.0f / 90.0f;
  CHECK(tm[0] == doctest::Approx(s));
  CHECK(tm[1] == doctest::Approx(0));
  CHECK(tm[3] == doctest::Approx(0));
  CHECK(tm[4] == doctest::Approx(s));
  CHECK(tm[0] * 120 + tm[2] == doctest::Approx(48));
  CHECK(tm[4] * 80 + tm[5] == doctest::Approx(48));
}

TEST_CASE("genderage crop pixels: center pixel comes from the box center, outside is black") {
  const int W = 320, H = 240;
  std::vector<uint8_t> img(W * H * 3, 0);
  for (int y = 70; y < 90; y++)
    for (int x = 110; x < 130; x++) img[(y * W + x) * 3 + 1] = 200;  // green square at the center
  FaceBox b{100, 50, 40, 60, 0.9f, {}};
  std::vector<uint8_t> out(96 * 96 * 3, 7);
  genderage_crop(img.data(), W, H, b, out.data());
  CHECK(out[(48 * 96 + 48) * 3 + 1] > 150);
  FaceBox edge{0, 0, 40, 40, 0.9f, {}};  // half the crop falls outside the image
  genderage_crop(img.data(), W, H, edge, out.data());
  CHECK(out[0] == 0);
}

TEST_CASE("decode genderage") {
  const float male[3] = {-1.0f, 2.0f, 0.37f};
  GenderAgeResult r = decode_genderage(male);
  CHECK(r.gender == 1);
  CHECK(r.age == 37);
  CHECK(r.margin == doctest::Approx(3.0f));
  const float female[3] = {0.5f, 0.2f, 0.064f};
  r = decode_genderage(female);
  CHECK(r.gender == 0);
  CHECK(r.age == 6);
}

TEST_CASE("age groups") {
  CHECK(std::string(age_group(5)) == "child");
  CHECK(std::string(age_group(12)) == "child");
  CHECK(std::string(age_group(13)) == "young");
  CHECK(std::string(age_group(29)) == "young");
  CHECK(std::string(age_group(30)) == "adult");
  CHECK(std::string(age_group(54)) == "adult");
  CHECK(std::string(age_group(55)) == "elder");
}

TEST_CASE("genderage model loads and infers") {
  GenderAge ga;
  CHECK_FALSE(ga.load("/nonexistent/x", 1));
  REQUIRE(ga.load("../research/2026-11-09-genderage/genderage", 1));
  std::vector<uint8_t> face(96 * 96 * 3, 128);
  float out[3];
  GenderAgeResult r = ga.infer(face.data(), out);
  CHECK(r.age >= 0);
  CHECK(r.age <= 120);
}
