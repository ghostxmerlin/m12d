// detect_file <model_base> <in.yuyv>: run the m12d detection path on one 640x480 YUYV frame.
// Prints one line per face: "face x y w h prob lm0x lm0y ... lm4x lm4y", coordinates scaled
// to 640x480, 3 decimals.
#include <cstdio>
#include <vector>
#include "image/yuyv.h"
#include "vision/retinaface.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: detect_file <model_base> <in.yuyv>\n");
    return 1;
  }
  const int W = 640, H = 480;
  std::vector<uint8_t> yuyv(W * H * 2);
  FILE* fp = std::fopen(argv[2], "rb");
  if (!fp || std::fread(yuyv.data(), 1, yuyv.size(), fp) != yuyv.size()) {
    std::fprintf(stderr, "detect_file: cannot read 640x480 YUYV from %s\n", argv[2]);
    return 3;
  }
  std::fclose(fp);
  std::vector<uint8_t> rgb((W / 2) * (H / 2) * 3);
  yuyv_to_rgb_half(yuyv.data(), W, H, rgb.data());
  RetinaFace rf;
  if (!rf.load(argv[1], 2)) {
    std::fprintf(stderr, "detect_file: cannot load model %s\n", argv[1]);
    return 2;
  }
  const float s = 2.0f;
  for (const FaceBox& b : rf.detect(rgb.data(), W / 2, H / 2)) {
    std::printf("face %.3f %.3f %.3f %.3f %.3f", b.x * s, b.y * s, b.w * s, b.h * s, b.prob);
    for (int k = 0; k < 5; k++) std::printf(" %.3f %.3f", b.lm[k][0] * s, b.lm[k][1] * s);
    std::printf("\n");
  }
  return 0;
}
