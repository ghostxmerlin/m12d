// m12cap <n> <out> [fps] [--every k]: capture n YUYV 640x480 frames and report fps.
// Without --every, saves the last frame to <out>. With --every k, saves every k-th frame
// to <out>_NN.yuyv (NN = 00, 01, ...).
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include "camera/capture_loop.h"
#include "camera/v4l2_camera.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: m12cap <n> <out> [fps] [--every k]\n");
    return 1;
  }
  const int n = std::atoi(argv[1]);
  const char* out_path = argv[2];
  int fps = 15, every = 0;
  for (int i = 3; i < argc; i++) {
    if (std::strcmp(argv[i], "--every") == 0 && i + 1 < argc) {
      every = std::atoi(argv[++i]);
    } else {
      fps = std::atoi(argv[i]);
    }
  }
  auto save = [](const Frame& fr, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    bool ok = fp && std::fwrite(fr.yuyv.data(), 1, fr.yuyv.size(), fp) == fr.yuyv.size();
    if (fp) std::fclose(fp);
    if (!ok) std::fprintf(stderr, "m12cap: cannot write %s\n", path.c_str());
    return ok;
  };
  int saved = 0;
  V4L2Camera cam("/dev/video0", 640, 480, fps);
  LatestFrame slot;
  std::atomic<bool> stop{false};
  CaptureStats st;
  std::thread t([&] { capture_loop(cam, slot, stop, st); });

  Frame f;
  uint64_t last = 0;
  int got = 0;
  std::chrono::steady_clock::time_point t0;
  while (got < n) {
    if (!slot.wait_newer(last, f, 5000)) {
      std::fprintf(stderr, "m12cap: no frame for 5s\n");
      break;
    }
    if (got == 0) t0 = std::chrono::steady_clock::now();
    last = f.id;
    got++;
    if (every > 0 && got % every == 0) {
      char name[16];
      std::snprintf(name, sizeof name, "_%02d.yuyv", saved++);
      if (!save(f, std::string(out_path) + name)) return 4;
    }
  }
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  stop = true;
  t.join();
  std::printf("cap frames=%d fps=%.1f reopens=%llu\n", got, got > 1 ? (got - 1) / secs : 0.0,
              static_cast<unsigned long long>(st.reopens.load()));
  if (got == 0) return 3;
  if (every > 0) {
    std::printf("saved=%d\n", saved);
  } else if (!save(f, out_path)) {
    return 4;
  }
  return 0;
}
