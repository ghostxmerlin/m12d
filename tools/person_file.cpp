// person_file <yolo_base> <face_base> <in.yuyv>...: phase-1b experiment tool.
// Each 640x480 frame -> 320x240 RGB (m12d path), then
//   - Yolo-Fastest (stock yolo_20class, VOC) stretched to SxS for S in {160, 224, 320}
//     (stock preprocessing: no letterbox, RGB / 255); prints every detection;
//   - RetinaFace at 192x144 (m12d default) for comparison.
// Output lines (coordinates at 640x480):
//   <file> yolo<S> <label> <prob> <x> <y> <w> <h>
//   <file> face192 <prob> <x> <y> <w> <h>
//   <file> yolo<S>_ms <ms>                       (host time, for relative cost only)
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>
#include "image/yuyv.h"
#include "mat.h"
#include "net.h"
#include "vision/retinaface.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: person_file <yolo_base> <face_base> <in.yuyv>...\n");
    return 1;
  }
  ncnn::Net yolo;
  yolo.opt.num_threads = 2;
  const std::string yb = argv[1];
  if (yolo.load_param((yb + ".param").c_str()) || yolo.load_model((yb + ".bin").c_str())) return 2;
  RetinaFace rf;
  if (!rf.load(argv[2], 2)) return 2;
  const int sizes[] = {160, 224, 320};
  std::vector<uint8_t> yuyv(640 * 480 * 2), rgb(320 * 240 * 3), small(192 * 144 * 3);
  for (int i = 3; i < argc; i++) {
    FILE* fp = std::fopen(argv[i], "rb");
    if (!fp || std::fread(yuyv.data(), 1, yuyv.size(), fp) != yuyv.size()) return 3;
    std::fclose(fp);
    yuyv_to_rgb_half(yuyv.data(), 640, 480, rgb.data());
    for (int s : sizes) {
      auto t0 = std::chrono::steady_clock::now();
      ncnn::Mat in = ncnn::Mat::from_pixels_resize(rgb.data(), ncnn::Mat::PIXEL_RGB, 320, 240, s, s);
      const float mean[3] = {0.f, 0.f, 0.f}, norm[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
      in.substract_mean_normalize(mean, norm);
      ncnn::Extractor ex = yolo.create_extractor();
      ex.input("data", in);
      ncnn::Mat out;
      ex.extract("output", out);
      double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      for (int r = 0; r < out.h; r++) {
        const float* v = out.row(r);
        std::printf("%s yolo%d %d %.3f %.1f %.1f %.1f %.1f\n", argv[i], s, (int)v[0], v[1], v[2] * 640,
                    v[3] * 480, (v[4] - v[2]) * 640, (v[5] - v[3]) * 480);
      }
      std::printf("%s yolo%d_ms %.1f\n", argv[i], s, ms);
    }
    ncnn::resize_bilinear_c3(rgb.data(), 320, 240, small.data(), 192, 144);
    for (auto& f : rf.detect(small.data(), 192, 144)) {
      const float k = 640.0f / 192;
      std::printf("%s face192 %.3f %.1f %.1f %.1f %.1f\n", argv[i], f.prob, f.x * k, f.y * k, f.w * k, f.h * k);
    }
  }
  return 0;
}
