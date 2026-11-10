#include <cstdio>
#include <fstream>
#include <vector>
#include "doctest.h"
#include "image/yuyv.h"
#include "vision/person_det.h"

static const char* kYolo = "../research/2026-11-10-person/yolo-fastest";

namespace {
float iou(const PersonBox& a, const PersonBox& b) {
  const float iw = std::max(0.f, std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x));
  const float ih = std::max(0.f, std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y));
  const float i = iw * ih;
  return i / (a.w * a.h + b.w * b.h - i);
}
// body_full frame -> 320x240 RGB; false (and SKIPPED) when the capture set is absent.
bool load_frame(const char* name, std::vector<uint8_t>& rgb) {
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

TEST_CASE("decode keeps only label 15 above threshold") {
  const float rows[] = {15, 0.9f, 0.1f, 0.2f, 0.3f, 0.8f,   //
                        9, 0.95f, 0.1f, 0.2f, 0.3f, 0.8f,   //
                        15, 0.5f, 0.1f, 0.2f, 0.3f, 0.8f};
  auto b = decode_person_rows(rows, 3, 320, 240, 0.6f);
  REQUIRE(b.size() == 1);
  CHECK(b[0].x == doctest::Approx(32));
  CHECK(b[0].y == doctest::Approx(48));
  CHECK(b[0].w == doctest::Approx(64));
  CHECK(b[0].h == doctest::Approx(144));
  CHECK(b[0].prob == doctest::Approx(0.9f));
}

TEST_CASE("decode sorts by prob desc") {
  const float rows[] = {15, 0.7f, 0.1f, 0.1f, 0.2f, 0.2f,   //
                        15, 0.95f, 0.5f, 0.5f, 0.6f, 0.6f,  //
                        15, 0.8f, 0.3f, 0.3f, 0.4f, 0.4f};
  auto b = decode_person_rows(rows, 3, 320, 240, 0.6f);
  REQUIRE(b.size() == 3);
  CHECK(b[0].prob == doctest::Approx(0.95f));
  CHECK(b[1].prob == doctest::Approx(0.8f));
  CHECK(b[2].prob == doctest::Approx(0.7f));
}

TEST_CASE("person detector load fails on missing model") {
  PersonDetector d;
  CHECK_FALSE(d.load("/nonexistent/x", 1));
}

TEST_CASE("160 finds the standing person at 2.5 m") {
  std::vector<uint8_t> rgb;
  if (!load_frame("stand25_00", rgb)) return;
  PersonDetector d;
  REQUIRE(d.load(kYolo, 1));
  auto b = d.detect(rgb.data(), 320, 240, 160, 0.6f);
  REQUIRE_FALSE(b.empty());
  const PersonBox gt{139.6f, 31.6f, 48.5f, 169.7f, 1.f};
  CHECK(iou(b[0], gt) >= 0.5f);
}

TEST_CASE("320 sees no person in the coat on the chair") {
  std::vector<uint8_t> rgb;
  if (!load_frame("clothes_00", rgb)) return;
  PersonDetector d;
  REQUIRE(d.load(kYolo, 1));
  CHECK(d.detect(rgb.data(), 320, 240, 320, 0.55f).empty());
}
