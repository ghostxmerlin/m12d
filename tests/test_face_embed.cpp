#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>
#include "doctest.h"
#include "image/yuyv.h"
#include "vision/face_embed.h"

static const char* kMfn =
    "../research/2026-11-09-app-readiness/official-recovery-files/home/m5stack/payload/models/mobilefacenet";
static const char* kRf =
    "../research/2026-11-09-app-readiness/official-recovery-files/home/m5stack/payload/models/mnet.25-opt";

namespace {
// Largest face of a faceid capture, or false (SKIPPED) without the capture set.
bool face_of(const char* name, std::vector<uint8_t>& rgb, FaceBox& face) {
  std::string p = std::string("tests/data/faceid/") + name + ".yuyv";
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
  RetinaFace rf;
  REQUIRE(rf.load(kRf, 1));
  auto faces = rf.detect(rgb.data(), 320, 240);
  REQUIRE_FALSE(faces.empty());
  face = faces[0];
  for (auto& g : faces)
    if (g.w * g.h > face.w * face.h) face = g;
  return true;
}
}  // namespace

TEST_CASE("cosine of identical unit vectors is 1") {
  Embedding a{};
  a[3] = 1.f;
  CHECK(cosine(a, a) == doctest::Approx(1.f));
}

TEST_CASE("embedder load fails on missing model") {
  FaceEmbedder e;
  CHECK_FALSE(e.load("/nonexistent/x", 1));
}

TEST_CASE("embedding is unit length") {
  std::vector<uint8_t> rgb;
  FaceBox face;
  if (!face_of("id_front_00", rgb, face)) return;
  FaceEmbedder e;
  REQUIRE(e.load(kMfn, 1));
  Embedding v = e.embed(rgb.data(), 320, 240, face);
  double n = 0;
  for (float x : v) n += double(x) * x;
  CHECK(std::sqrt(n) == doctest::Approx(1.0).epsilon(1e-4));
}

TEST_CASE("same person different frames is similar") {
  std::vector<uint8_t> a, b;
  FaceBox fa, fb;
  if (!face_of("id_front_00", a, fa) || !face_of("id_front_01", b, fb)) return;
  FaceEmbedder e;
  REQUIRE(e.load(kMfn, 1));
  CHECK(cosine(e.embed(a.data(), 320, 240, fa), e.embed(b.data(), 320, 240, fb)) > 0.6f);
}
