// m12cap <n> <out.yuyv> [fps]: capture n YUYV 640x480 frames, report fps, save the last frame.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include "camera/capture_loop.h"
#include "camera/v4l2_camera.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: m12cap <n> <out.yuyv> [fps]\n");
    return 1;
  }
  const int n = std::atoi(argv[1]);
  const char* out_path = argv[2];
  const int fps = argc > 3 ? std::atoi(argv[3]) : 15;
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
  }
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  stop = true;
  t.join();
  std::printf("cap frames=%d fps=%.1f reopens=%llu\n", got, got > 1 ? (got - 1) / secs : 0.0,
              static_cast<unsigned long long>(st.reopens.load()));
  if (got == 0) return 3;
  FILE* fp = std::fopen(out_path, "wb");
  if (!fp || std::fwrite(f.yuyv.data(), 1, f.yuyv.size(), fp) != f.yuyv.size()) {
    std::fprintf(stderr, "m12cap: cannot write %s\n", out_path);
    return 4;
  }
  std::fclose(fp);
  return 0;
}
