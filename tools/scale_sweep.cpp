// scale_sweep <model_base> <in.yuyv>...: detection at several input sizes (experiment tool).
// Each 640x480 frame -> 320x240 RGB (m12d path) -> bilinear resize to WxH -> RetinaFace.
// Prints: frame size num best_prob face_w_px(at 640x480) for each size.
#include <cstdio>
#include <vector>
#include "image/yuyv.h"
#include "mat.h"
#include "vision/retinaface.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: scale_sweep <model_base> <in.yuyv>...\n");
    return 1;
  }
  RetinaFace rf;
  if (!rf.load(argv[1], 2)) return 2;
  const int sizes[][2] = {{320, 240}, {256, 192}, {224, 168}, {192, 144}, {160, 120}};
  std::vector<uint8_t> yuyv(640 * 480 * 2), rgb(320 * 240 * 3), small(320 * 240 * 3);
  for (int i = 2; i < argc; i++) {
    FILE* fp = std::fopen(argv[i], "rb");
    if (!fp || std::fread(yuyv.data(), 1, yuyv.size(), fp) != yuyv.size()) return 3;
    std::fclose(fp);
    yuyv_to_rgb_half(yuyv.data(), 640, 480, rgb.data());
    for (auto& s : sizes) {
      const int w = s[0], h = s[1];
      const uint8_t* in = rgb.data();
      if (w != 320) {
        ncnn::resize_bilinear_c3(rgb.data(), 320, 240, small.data(), w, h);
        in = small.data();
      }
      auto faces = rf.detect(in, w, h);
      float best = 0, fw = 0;
      for (auto& f : faces)
        if (f.prob > best) {
          best = f.prob;
          fw = f.w * 640.0f / w;
        }
      std::printf("%s %dx%d num=%zu prob=%.3f face_w=%.0f\n", argv[i], w, h, faces.size(), best, fw);
    }
  }
  return 0;
}
