// faceid_eval <mnet_base> <mfn_base> <in.yuyv>... : phase-2 offline evaluation helper (host).
// For every face RetinaFace finds (320x240, prob >= 0.8) prints one line:
//   <file> <idx> <x> <y> <w> <h> <prob> <128 floats>     (box in 640x480 coordinates)
// Output contains biometric features: write it to a scratch dir, never into the repo.
#include <cstdio>
#include <vector>
#include "image/yuyv.h"
#include "vision/face_embed.h"
#include "vision/retinaface.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: faceid_eval <mnet_base> <mfn_base> <in.yuyv>...\n");
    return 1;
  }
  RetinaFace rf;
  FaceEmbedder fe;
  if (!rf.load(argv[1], 2) || !fe.load(argv[2], 2)) return 2;
  std::vector<uint8_t> yuyv(640 * 480 * 2), rgb(320 * 240 * 3);
  for (int i = 3; i < argc; i++) {
    FILE* fp = std::fopen(argv[i], "rb");
    if (!fp || std::fread(yuyv.data(), 1, yuyv.size(), fp) != yuyv.size()) {
      std::fprintf(stderr, "faceid_eval: cannot read %s\n", argv[i]);
      if (fp) std::fclose(fp);
      continue;
    }
    std::fclose(fp);
    yuyv_to_rgb_half(yuyv.data(), 640, 480, rgb.data());
    auto faces = rf.detect(rgb.data(), 320, 240);
    for (size_t k = 0; k < faces.size(); k++) {
      const FaceBox& f = faces[k];
      std::printf("%s %zu %.1f %.1f %.1f %.1f %.3f", argv[i], k, f.x * 2, f.y * 2, f.w * 2, f.h * 2, f.prob);
      for (float v : fe.embed(rgb.data(), 320, 240, f)) std::printf(" %.6f", v);
      std::printf("\n");
    }
  }
  return 0;
}
