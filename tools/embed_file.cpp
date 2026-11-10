// embed_file <mnet_base> <mfn_base> <in.yuyv> [--dump-rgb out.rgb] [--int-lm]: phase-2 A-layer helper.
// Detects the largest face (RetinaFace at 320x240), prints "lm x0 y0 .. x4 y4" (ints, truncated like
// stock FaceInfo::landmark) and then the 128-d FaceEmbedder feature, one value per line.
// --int-lm embeds with the truncated landmarks, so the input matches ref_stock_arc exactly.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include "image/yuyv.h"
#include "vision/face_embed.h"
#include "vision/retinaface.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: embed_file <mnet_base> <mfn_base> <in.yuyv> [--dump-rgb out.rgb] [--int-lm]\n");
    return 1;
  }
  const char* dump = nullptr;
  bool int_lm = false;
  for (int i = 4; i < argc; i++) {
    if (!std::strcmp(argv[i], "--dump-rgb") && i + 1 < argc) dump = argv[++i];
    else if (!std::strcmp(argv[i], "--int-lm")) int_lm = true;
  }
  std::vector<uint8_t> yuyv(640 * 480 * 2), rgb(320 * 240 * 3);
  FILE* fp = std::fopen(argv[3], "rb");
  if (!fp || std::fread(yuyv.data(), 1, yuyv.size(), fp) != yuyv.size()) return 3;
  std::fclose(fp);
  yuyv_to_rgb_half(yuyv.data(), 640, 480, rgb.data());
  if (dump) {
    FILE* o = std::fopen(dump, "wb");
    if (!o || std::fwrite(rgb.data(), 1, rgb.size(), o) != rgb.size()) return 3;
    std::fclose(o);
  }
  RetinaFace rf;
  FaceEmbedder fe;
  if (!rf.load(argv[1], 2) || !fe.load(argv[2], 2)) return 2;
  auto faces = rf.detect(rgb.data(), 320, 240);
  if (faces.empty()) return 4;
  FaceBox f = faces[0];
  for (auto& g : faces)
    if (g.w * g.h > f.w * f.h) f = g;
  std::printf("lm");
  for (int k = 0; k < 5; k++) {
    std::printf(" %d %d", (int)f.lm[k][0], (int)f.lm[k][1]);
    if (int_lm) {
      f.lm[k][0] = (float)(int)f.lm[k][0];
      f.lm[k][1] = (float)(int)f.lm[k][1];
    }
  }
  std::printf("\n");
  Embedding e{};
  for (int r = 0; r < 5; r++) {  // first run warms up; stderr gets each run's time
    const auto t0 = std::chrono::steady_clock::now();
    e = fe.embed(rgb.data(), 320, 240, f);
    std::fprintf(stderr, "embed_ms %.1f\n",
                 std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
  }
  for (float v : e) std::printf("%.6f\n", v);
  return 0;
}
