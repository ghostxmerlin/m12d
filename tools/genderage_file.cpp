// genderage_file <det_model> <ga_model> <out_dir> <in.yuyv>...: insightface genderage on the
// best face of each 640x480 frame (validation tool). Crops from the 320x240 path and from
// full 640x480, writes <out>/<name>_{320,640}.rgb (96x96 RGB) and .out (3 floats, text).
#include <cstdio>
#include <string>
#include <vector>
#include "image/yuyv.h"
#include "vision/genderage.h"
#include "vision/retinaface.h"

static std::string stem(const std::string& p) {
  size_t a = p.find_last_of('/'), b = p.rfind('.');
  size_t s = a == std::string::npos ? 0 : a + 1;
  return p.substr(s, b - s);
}

int main(int argc, char** argv) {
  if (argc < 5) {
    std::fprintf(stderr, "usage: genderage_file <det_model> <ga_model> <out_dir> <in.yuyv>...\n");
    return 1;
  }
  RetinaFace rf;
  GenderAge ga;
  if (!rf.load(argv[1], 2) || !ga.load(argv[2], 2)) return 2;
  const std::string out = argv[3];
  std::vector<uint8_t> yuyv(640 * 480 * 2), rgb320(320 * 240 * 3), rgb640(640 * 480 * 3), crop(96 * 96 * 3);
  for (int i = 4; i < argc; i++) {
    FILE* fp = std::fopen(argv[i], "rb");
    if (!fp || std::fread(yuyv.data(), 1, yuyv.size(), fp) != yuyv.size()) return 3;
    std::fclose(fp);
    yuyv_to_rgb_half(yuyv.data(), 640, 480, rgb320.data());
    yuyv_to_rgb(yuyv.data(), 640, 480, rgb640.data());
    auto faces = rf.detect(rgb320.data(), 320, 240);
    const std::string name = stem(argv[i]);
    if (faces.empty()) {
      std::printf("%s noface\n", name.c_str());
      continue;
    }
    std::printf("%-16s w=%3.0f", name.c_str(), faces[0].w * 2);
    for (int res : {320, 640}) {
      std::vector<FaceBox> b = {faces[0]};
      if (res == 640) scale_faces(b, 2.0f);
      genderage_crop(res == 320 ? rgb320.data() : rgb640.data(), res, res * 3 / 4, b[0], crop.data());
      float o[3];
      GenderAgeResult r = ga.infer(crop.data(), o);
      const std::string base = out + "/" + name + "_" + std::to_string(res);
      FILE* f = std::fopen((base + ".rgb").c_str(), "wb");
      std::fwrite(crop.data(), 1, crop.size(), f);
      std::fclose(f);
      f = std::fopen((base + ".out").c_str(), "w");
      std::fprintf(f, "%.6f\n%.6f\n%.6f\n", o[0], o[1], o[2]);
      std::fclose(f);
      std::printf("   src%d: %s age=%2d (%s) margin=%.2f", res, r.gender == 1 ? "M" : "F", r.age, age_group(r.age),
                  r.margin);
    }
    std::printf("\n");
  }
  return 0;
}
