#include <cstdio>
#include <fstream>
#include <vector>
#include "doctest.h"
#include "image/yuyv.h"
#include "vision/retinaface.h"

static const char* kModel =
    "../research/2026-11-09-app-readiness/official-recovery-files/home/m5stack/payload/models/mnet.25-opt";

TEST_CASE("load fails on missing model") {
  RetinaFace rf;
  CHECK_FALSE(rf.load("/nonexistent/x", 1));
}

TEST_CASE("load succeeds on stock model") {
  RetinaFace rf;
  CHECK(rf.load(kModel, 1));
}

TEST_CASE("no face on uniform gray") {
  RetinaFace rf;
  REQUIRE(rf.load(kModel, 1));
  std::vector<uint8_t> rgb(320 * 240 * 3, 128);
  CHECK(rf.detect(rgb.data(), 320, 240).empty());
}

TEST_CASE("finds face in captured frame") {
  std::ifstream f("tests/data/face_640x480.yuyv", std::ios::binary);
  if (!f) {
    MESSAGE("SKIPPED: no tests/data/face_640x480.yuyv");
    std::printf("SKIPPED: no tests/data/face_640x480.yuyv\n");
    return;
  }
  std::vector<uint8_t> yuyv(640 * 480 * 2);
  f.read(reinterpret_cast<char*>(yuyv.data()), yuyv.size());
  REQUIRE(f.gcount() == static_cast<std::streamsize>(yuyv.size()));
  std::vector<uint8_t> rgb(320 * 240 * 3);
  yuyv_to_rgb_half(yuyv.data(), 640, 480, rgb.data());

  RetinaFace rf;
  REQUIRE(rf.load(kModel, 1));
  auto faces = rf.detect(rgb.data(), 320, 240);
  REQUIRE(faces.size() >= 1);
  const FaceBox& b = faces[0];
  CHECK(b.prob > 0.9f);
  CHECK(b.x >= 0);
  CHECK(b.y >= 0);
  CHECK(b.x + b.w <= 320);
  CHECK(b.y + b.h <= 240);
  for (int k = 0; k < 5; k++) {
    CHECK(b.lm[k][0] >= b.x);
    CHECK(b.lm[k][0] <= b.x + b.w);
    CHECK(b.lm[k][1] >= b.y);
    CHECK(b.lm[k][1] <= b.y + b.h);
  }
}

TEST_CASE("scale_faces maps detector coordinates to another resolution") {
  FaceBox b{10, 20, 30, 40, 0.9f, {{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}}};
  std::vector<FaceBox> v = {b};
  scale_faces(v, 2.5f);
  CHECK(v[0].x == doctest::Approx(25));
  CHECK(v[0].y == doctest::Approx(50));
  CHECK(v[0].w == doctest::Approx(75));
  CHECK(v[0].h == doctest::Approx(100));
  CHECK(v[0].prob == doctest::Approx(0.9f));
  CHECK(v[0].lm[4][0] == doctest::Approx(22.5));
  CHECK(v[0].lm[4][1] == doctest::Approx(25));
}

TEST_CASE("finds face at 192x144 after bilinear resize") {
  std::ifstream f("tests/data/face_640x480.yuyv", std::ios::binary);
  if (!f) {
    MESSAGE("SKIPPED: no tests/data/face_640x480.yuyv");
    return;
  }
  std::vector<uint8_t> yuyv(640 * 480 * 2), rgb(320 * 240 * 3), small(192 * 144 * 3);
  f.read(reinterpret_cast<char*>(yuyv.data()), yuyv.size());
  yuyv_to_rgb_half(yuyv.data(), 640, 480, rgb.data());
  resize_rgb(rgb.data(), 320, 240, small.data(), 192, 144);
  RetinaFace rf;
  REQUIRE(rf.load(kModel, 1));
  auto faces = rf.detect(small.data(), 192, 144);
  REQUIRE(faces.size() == 1);
  scale_faces(faces, 320.0f / 192);
  // same face as at 320x240 (345,136,97x119 at 640x480 -> /2), within a few pixels
  CHECK(faces[0].x == doctest::Approx(172.5).epsilon(0.03));
  CHECK(faces[0].y == doctest::Approx(68.1).epsilon(0.05));
}
