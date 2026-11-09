#include <cmath>
#include <cstdlib>
#include <tuple>
#include <vector>
#include "doctest.h"
#include "image/yuyv.h"

// Build a w x h YUYV image where every pixel has luma y and chroma u/v.
static std::vector<uint8_t> solid(int w, int h, uint8_t y, uint8_t u, uint8_t v) {
  std::vector<uint8_t> img(static_cast<size_t>(w) * h * 2);
  for (size_t i = 0; i < img.size(); i += 4) {
    img[i] = y;
    img[i + 1] = u;
    img[i + 2] = y;
    img[i + 3] = v;
  }
  return img;
}

static bool near(int a, int b, int tol) { return std::abs(a - b) <= tol; }

TEST_CASE("white") {
  auto img = solid(4, 2, 235, 128, 128);
  std::vector<uint8_t> rgb(2 * 1 * 3);
  yuyv_to_rgb_half(img.data(), 4, 2, rgb.data());
  for (uint8_t c : rgb) CHECK(near(c, 255, 1));
}

TEST_CASE("black") {
  auto img = solid(4, 2, 16, 128, 128);
  std::vector<uint8_t> rgb(2 * 1 * 3);
  yuyv_to_rgb_half(img.data(), 4, 2, rgb.data());
  for (uint8_t c : rgb) CHECK(near(c, 0, 1));
}

TEST_CASE("red") {
  auto img = solid(4, 2, 81, 90, 240);
  std::vector<uint8_t> rgb(2 * 1 * 3);
  yuyv_to_rgb_half(img.data(), 4, 2, rgb.data());
  for (int p = 0; p < 2; p++) {
    CHECK(near(rgb[p * 3 + 0], 255, 3));
    CHECK(near(rgb[p * 3 + 1], 0, 3));
    CHECK(near(rgb[p * 3 + 2], 0, 3));
  }
}

TEST_CASE("averages 2x2 luma") {
  // One 2x2 block: each row is one macropixel Y0=16, Y1=235.
  std::vector<uint8_t> img = {16, 128, 235, 128, 16, 128, 235, 128};
  std::vector<uint8_t> rgb(3);
  yuyv_to_rgb_half(img.data(), 2, 2, rgb.data());
  int expect = static_cast<int>(std::lround(1.164 * (125.5 - 16)));
  for (uint8_t c : rgb) CHECK(near(c, expect, 1));
}

TEST_CASE("output size") {
  auto img = solid(640, 480, 120, 128, 128);
  std::vector<uint8_t> rgb(320 * 240 * 3 + 16, 0xAB);
  yuyv_to_rgb_half(img.data(), 640, 480, rgb.data());
  for (size_t i = 320 * 240 * 3; i < rgb.size(); i++) CHECK(rgb[i] == 0xAB);
  CHECK(rgb[320 * 240 * 3 - 1] != 0xAB);
}

TEST_CASE("full-scale: white black red") {
  for (auto [y, u, v, r, g, b, tol] : {std::tuple<int, int, int, int, int, int, int>{235, 128, 128, 255, 255, 255, 1},
                                       {16, 128, 128, 0, 0, 0, 1},
                                       {81, 90, 240, 255, 0, 0, 3}}) {
    auto img = solid(4, 2, y, u, v);
    std::vector<uint8_t> rgb(4 * 2 * 3);
    yuyv_to_rgb(img.data(), 4, 2, rgb.data());
    for (int p = 0; p < 8; p++) {
      CHECK(near(rgb[p * 3 + 0], r, tol));
      CHECK(near(rgb[p * 3 + 1], g, tol));
      CHECK(near(rgb[p * 3 + 2], b, tol));
    }
  }
}

TEST_CASE("full-scale keeps each pixel's own luma") {
  std::vector<uint8_t> img = {16, 128, 235, 128};  // one macropixel: Y0=16, Y1=235
  std::vector<uint8_t> rgb(2 * 3);
  yuyv_to_rgb(img.data(), 2, 1, rgb.data());
  for (int c = 0; c < 3; c++) {
    CHECK(near(rgb[c], 0, 1));
    CHECK(near(rgb[3 + c], 255, 1));
  }
}

TEST_CASE("full-scale output size") {
  auto img = solid(320, 240, 120, 128, 128);
  std::vector<uint8_t> rgb(320 * 240 * 3 + 16, 0xAB);
  yuyv_to_rgb(img.data(), 320, 240, rgb.data());
  for (size_t i = 320 * 240 * 3; i < rgb.size(); i++) CHECK(rgb[i] == 0xAB);
  CHECK(rgb[320 * 240 * 3 - 1] != 0xAB);
}
