// attr_file <det_model> <ag_model> <out_dir> <in.yuyv>...: age/gender on the best face of
// each 640x480 frame (validation tool). For each frame, aligns from the 320x240 path
// (what m12d sees) and from full 640x480, writes <out>/<name>_{320,640}.rgb (112x112 RGB)
// and .fc1 (202 floats, text), and prints the decoded results.
#include <cstdio>
#include <string>
#include <vector>
#include "image/yuyv.h"
#include "vision/age_gender.h"
#include "vision/align.h"
#include "vision/retinaface.h"

static std::string stem(const std::string& p) {
  size_t a = p.find_last_of('/'), b = p.rfind('.');
  return p.substr(a == std::string::npos ? 0 : a + 1, b - (a == std::string::npos ? 0 : a + 1));
}

int main(int argc, char** argv) {
  if (argc < 5) {
    std::fprintf(stderr, "usage: attr_file <det_model> <ag_model> <out_dir> <in.yuyv>...\n");
    return 1;
  }
  RetinaFace rf;
  AgeGender ag;
  if (!rf.load(argv[1], 2) || !ag.load(argv[2], 2)) return 2;
  const std::string out = argv[3];
  std::vector<uint8_t> yuyv(640 * 480 * 2), rgb320(320 * 240 * 3), rgb640(640 * 480 * 3), crop(112 * 112 * 3);
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
    const FaceBox& b = faces[0];
    std::printf("%s face_w640=%.0f", name.c_str(), b.w * 2);
    for (int res : {320, 640}) {
      float lm[5][2];
      for (int k = 0; k < 5; k++) {
        lm[k][0] = b.lm[k][0] * (res / 320.0f);
        lm[k][1] = b.lm[k][1] * (res / 320.0f);
      }
      align_face(res == 320 ? rgb320.data() : rgb640.data(), res, res * 3 / 4, lm, crop.data());
      std::vector<float> fc1;
      AgeGenderResult r = ag.infer(crop.data(), &fc1);
      const std::string base = out + "/" + name + "_" + std::to_string(res);
      FILE* f = std::fopen((base + ".rgb").c_str(), "wb");
      std::fwrite(crop.data(), 1, crop.size(), f);
      std::fclose(f);
      f = std::fopen((base + ".fc1").c_str(), "w");
      for (float v : fc1) std::fprintf(f, "%.6f\n", v);
      std::fclose(f);
      std::printf("  src%d: age=%d gender=%d (g0=%.3f g1=%.3f)", res, r.age, r.gender, fc1[0], fc1[1]);
    }
    std::printf("\n");
  }
  return 0;
}
