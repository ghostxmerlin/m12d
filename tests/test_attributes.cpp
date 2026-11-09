#include <cmath>
#include <vector>
#include "doctest.h"
#include "vision/age_gender.h"
#include "vision/align.h"

namespace {
// 112x112 ArcFace template (x already shifted by +8).
const float kT[5][2] = {{38.2946f, 51.6963f}, {73.5318f, 51.5014f}, {56.0252f, 71.7366f},
                        {41.5493f, 92.3655f}, {70.7299f, 92.2041f}};
const uint8_t kColor[5][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {0, 255, 255}};

void dot(std::vector<uint8_t>& img, int w, int h, float cx, float cy, int r, const uint8_t* c) {
  for (int y = int(cy) - r; y <= int(cy) + r; y++)
    for (int x = int(cx) - r; x <= int(cx) + r; x++)
      if (x >= 0 && y >= 0 && x < w && y < h)
        for (int k = 0; k < 3; k++) img[(y * w + x) * 3 + k] = c[k];
}
}  // namespace

TEST_CASE("align: landmarks land on the template") {
  // Face at half template scale, rotated 20 degrees, placed at (150, 90) in a 320x240 image.
  const int W = 320, H = 240;
  std::vector<uint8_t> img(W * H * 3, 40);
  float lm[5][2];
  const float a = 20.f * 3.14159265f / 180.f, s = 0.5f;
  for (int i = 0; i < 5; i++) {
    float x = kT[i][0] - 56, y = kT[i][1] - 56;
    lm[i][0] = 150 + s * (x * std::cos(a) - y * std::sin(a));
    lm[i][1] = 90 + s * (x * std::sin(a) + y * std::cos(a));
    dot(img, W, H, lm[i][0], lm[i][1], 2, kColor[i]);
  }
  std::vector<uint8_t> out(112 * 112 * 3);
  align_face(img.data(), W, H, lm, out.data());
  for (int i = 0; i < 5; i++) {
    const uint8_t* p = &out[(int(std::lround(kT[i][1])) * 112 + int(std::lround(kT[i][0]))) * 3];
    INFO("landmark " << i);
    for (int k = 0; k < 3; k++) CHECK(std::abs(int(p[k]) - int(kColor[i][k])) < 70);
  }
}

TEST_CASE("align: similarity maps source landmarks to the template") {
  float lm[5][2];
  for (int i = 0; i < 5; i++) {
    lm[i][0] = 10 + 0.4f * kT[i][0];
    lm[i][1] = 20 + 0.4f * kT[i][1];
  }
  float tm[6];
  similarity_to_template(lm, tm);  // maps image -> 112x112
  for (int i = 0; i < 5; i++) {
    CHECK(tm[0] * lm[i][0] + tm[1] * lm[i][1] + tm[2] == doctest::Approx(kT[i][0]).epsilon(0.001));
    CHECK(tm[3] * lm[i][0] + tm[4] * lm[i][1] + tm[5] == doctest::Approx(kT[i][1]).epsilon(0.001));
  }
}

TEST_CASE("decode age/gender from fc1") {
  std::vector<float> fc1(202, 0.f);
  fc1[0] = 0.1f;  // gender logits: index 1 wins
  fc1[1] = 0.9f;
  for (int k = 0; k < 100; k++) {  // pair k: (no, yes); first 37 pairs say "older than k"
    fc1[2 + 2 * k] = k < 37 ? 0.f : 1.f;
    fc1[2 + 2 * k + 1] = k < 37 ? 1.f : 0.f;
  }
  AgeGenderResult r = decode_age_gender(fc1.data());
  CHECK(r.age == 37);
  CHECK(r.gender == 1);
}

TEST_CASE("age/gender model loads and infers") {
  AgeGender ag;
  CHECK_FALSE(ag.load("/nonexistent/x", 1));
  REQUIRE(ag.load("../research/2026-11-09-age-gender/age_gender", 1));
  std::vector<uint8_t> face(112 * 112 * 3, 128);
  std::vector<float> fc1;
  AgeGenderResult r = ag.infer(face.data(), &fc1);
  CHECK(fc1.size() == 202);
  CHECK(r.age >= 0);
  CHECK(r.age <= 100);
}
