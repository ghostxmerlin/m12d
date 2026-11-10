#include <cstdio>
#include <fstream>
#include <vector>
#include "doctest.h"
#include "image/yuyv.h"
#include "vision/person_stage.h"

static const char* kYoloS = "../research/2026-11-10-person/yolo-fastest";

namespace {
bool frame(const char* name, std::vector<uint8_t>& rgb) {
  std::string p = std::string("tests/data/body_full/") + name + ".yuyv";
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    MESSAGE("SKIPPED: no " << p);
    std::printf("SKIPPED: no %s\n", p.c_str());
    return false;
  }
  std::vector<uint8_t> yuyv(640 * 480 * 2);
  f.read(reinterpret_cast<char*>(yuyv.data()), yuyv.size());
  REQUIRE(f.gcount() == static_cast<std::streamsize>(yuyv.size()));
  rgb.resize(320 * 240 * 3);
  yuyv_to_rgb_half(yuyv.data(), 640, 480, rgb.data());
  return true;
}
}  // namespace

TEST_CASE("coat on a chair never becomes a person") {
  std::vector<uint8_t> rgb;
  if (!frame("clothes_00", rgb)) return;
  PersonDetector det;
  REQUIRE(det.load(kYoloS, 1));
  PersonStage st(det);
  for (int i = 0; i < 10; i++) CHECK(st.step(rgb.data(), true, {}, {}, i * 100).empty());
}

TEST_CASE("standing person is reported from the first person frame, paired with a face") {
  std::vector<uint8_t> rgb;
  if (!frame("stand25_00", rgb)) return;
  PersonDetector det;
  REQUIRE(det.load(kYoloS, 1));
  PersonStage st(det);
  // A face whose center (164, 45) is in the top of the person (gt {139.6,31.6,48.5,169.7}).
  const std::vector<FaceBox> faces = {FaceBox{158, 38, 12, 14, 0.9f, {}}};
  auto a = st.step(rgb.data(), true, faces, {42}, 0);
  REQUIRE(a.size() == 1);
  CHECK(a[0].face_id == 42);
  auto b = st.step(rgb.data(), false, {}, {}, 100);  // no person run this frame: tracks held
  REQUIRE(b.size() == 1);
  CHECK(b[0].id == a[0].id);
  CHECK(b[0].face_id == -1);
  CHECK(st.step(rgb.data(), true, {}, {}, 200).size() == 1);
}

TEST_CASE("reset starts over with a new track") {
  std::vector<uint8_t> rgb;
  if (!frame("stand25_00", rgb)) return;
  PersonDetector det;
  REQUIRE(det.load(kYoloS, 1));
  PersonStage st(det);
  auto a = st.step(rgb.data(), true, {}, {}, 0);
  REQUIRE(a.size() == 1);
  st.reset();
  CHECK(st.step(rgb.data(), false, {}, {}, 100).empty());
  auto b = st.step(rgb.data(), true, {}, {}, 200);
  REQUIRE(b.size() == 1);
  CHECK(b[0].id != a[0].id);
}
