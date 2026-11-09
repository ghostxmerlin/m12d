// m12bench <model_base> <w> <h> <iters>: time full mnet.25 inference (all 9 outputs) on a constant input.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "net.h"

int main(int argc, char** argv) {
  if (argc != 5) {
    std::fprintf(stderr, "usage: m12bench <model_base> <w> <h> <iters>\n");
    return 1;
  }
  const std::string base = argv[1];
  const int w = std::atoi(argv[2]), h = std::atoi(argv[3]), iters = std::atoi(argv[4]);
  ncnn::Net net;
  net.opt.num_threads = 2;
  net.opt.lightmode = true;
  net.opt.openmp_blocktime = 0;
  if (net.load_param((base + ".param").c_str()) != 0 || net.load_model((base + ".bin").c_str()) != 0) {
    std::fprintf(stderr, "m12bench: failed to load %s.{param,bin}\n", base.c_str());
    return 2;
  }
  ncnn::Mat in(w, h, 3);
  in.fill(0.5f);
  auto run = [&] {
    ncnn::Extractor ex = net.create_extractor();
    ex.input("data", in);
    ncnn::Mat out;
    // All nine outputs, like the real detector (cls + bbox + landmark per stride).
    for (int s : {32, 16, 8})
      for (const char* kind : {"cls_prob_reshape", "bbox_pred", "landmark_pred"}) {
        std::string name = std::string("face_rpn_") + kind + "_stride" + std::to_string(s);
        ex.extract(name.c_str(), out);
      }
  };
  for (int i = 0; i < 2; i++) run();
  double sum = 0, mn = 1e9;
  for (int i = 0; i < iters; i++) {
    auto t0 = std::chrono::steady_clock::now();
    run();
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    sum += ms;
    if (ms < mn) mn = ms;
  }
  std::printf("bench model=%s in=%dx%d iters=%d avg_ms=%.1f min_ms=%.1f\n", base.c_str(), w, h, iters,
              sum / iters, mn);
  return 0;
}
