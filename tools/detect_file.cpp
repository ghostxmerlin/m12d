// detect_file: run the m12d detection path on one frame.
//   detect_file <model_base> <in.yuyv> [--dump-rgb out.rgb]   640x480 YUYV -> yuyv_to_rgb_half -> detect
//   detect_file <model_base> --rgb <in.rgb>                   320x240 packed RGB -> detect
// Prints one line per face: "face x y w h prob lm0x lm0y ... lm4x lm4y", coordinates scaled
// to 640x480, 3 decimals.
#include <cstdio>
#include <string>
#include <vector>
#include "image/yuyv.h"
#include "vision/retinaface.h"

static bool read_exact(const char* path, std::vector<uint8_t>& buf) {
  FILE* fp = std::fopen(path, "rb");
  bool ok = fp && std::fread(buf.data(), 1, buf.size(), fp) == buf.size();
  if (fp) std::fclose(fp);
  return ok;
}

int main(int argc, char** argv) {
  const int W = 640, H = 480;
  std::vector<uint8_t> rgb((W / 2) * (H / 2) * 3);
  const std::string mode = argc > 2 ? argv[2] : "";
  if (argc == 4 && mode == "--rgb") {
    if (!read_exact(argv[3], rgb)) {
      std::fprintf(stderr, "detect_file: cannot read 320x240 RGB from %s\n", argv[3]);
      return 3;
    }
  } else if (argc == 3 || (argc == 5 && std::string(argv[3]) == "--dump-rgb")) {
    std::vector<uint8_t> yuyv(W * H * 2);
    if (!read_exact(argv[2], yuyv)) {
      std::fprintf(stderr, "detect_file: cannot read 640x480 YUYV from %s\n", argv[2]);
      return 3;
    }
    yuyv_to_rgb_half(yuyv.data(), W, H, rgb.data());
    if (argc == 5) {
      FILE* fp = std::fopen(argv[4], "wb");
      if (!fp || std::fwrite(rgb.data(), 1, rgb.size(), fp) != rgb.size()) {
        std::fprintf(stderr, "detect_file: cannot write %s\n", argv[4]);
        return 4;
      }
      std::fclose(fp);
    }
  } else {
    std::fprintf(stderr,
                 "usage: detect_file <model_base> <in.yuyv> [--dump-rgb out.rgb]\n"
                 "       detect_file <model_base> --rgb <in.rgb>\n");
    return 1;
  }
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
