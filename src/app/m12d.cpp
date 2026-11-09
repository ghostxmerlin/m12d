// m12d — resident face detector for the UnitV2 M12 (phase 1).
// Threads: capture (V4L2 -> LatestFrame), detect (RetinaFace every frame -> push),
// io (serial + TCP, unified command set). Main thread: thermal guard + heartbeat.
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/options.h"
#include "camera/capture_loop.h"
#include "camera/v4l2_camera.h"
#include "core/latest_frame.h"
#include "image/yuyv.h"
#include "io/io_loop.h"
#include "io/serial_port.h"
#include "io/tcp_listener.h"
#include "proto/commands.h"
#include "proto/det_json.h"
#include "sys/sysinfo.h"
#include "sys/thermal.h"
#include "vision/retinaface.h"

#ifndef M12D_GIT
#define M12D_GIT "unknown"
#endif

namespace {
constexpr const char* kVersion = "0.1.0";
constexpr const char* kSnapPath = "/tmp/m12d_snap.yuyv";
constexpr float kScale = 2.0f;  // detector runs at 320x240, output in 640x480

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

using Clock = std::chrono::steady_clock;
int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

// Shared state read by the heartbeat / status command.
struct Shared {
  std::atomic<int> temp{-1};
  std::atomic<bool> paused{false};
  std::atomic<float> fps{0};
  std::atomic<uint64_t> det_frames{0};
};

// Latest detected frame and its faces, for the snap command.
struct Snapshot {
  std::mutex mu;
  Frame frame;
  std::vector<FaceBox> faces;
};

bool write_snap(const Frame& f) {
  int fd = ::open(kSnapPath, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0644);
  if (fd < 0) return false;
  size_t off = 0;
  while (off < f.yuyv.size()) {
    ssize_t n = ::write(fd, f.yuyv.data() + off, f.yuyv.size() - off);
    if (n <= 0) break;
    off += static_cast<size_t>(n);
  }
  ::close(fd);
  return off == f.yuyv.size();
}
}  // namespace

int main(int argc, char** argv) {
  std::string err;
  auto parsed = parse_args(argc, argv, err);
  if (!parsed) {
    std::fprintf(stderr, "m12d: %s\n", err.c_str());
    return 1;
  }
  const Options opt = *parsed;
  std::fprintf(stderr,
               "m12d %s (%s) models=%s serial=%s tcp=%s:%d stdout=%d fps_cap=%.1f cap_fps=%d max_faces=%d "
               "threads=%d hb_ms=%d\n",
               kVersion, M12D_GIT, opt.models.c_str(), opt.use_serial ? opt.serial.c_str() : "off",
               opt.tcp_bind.c_str(), opt.tcp_port, opt.use_stdout, opt.fps_cap, opt.cap_fps, opt.max_faces,
               opt.threads, opt.hb_ms);

  struct sigaction sa {};
  sa.sa_handler = on_signal;
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT, &sa, nullptr);
  signal(SIGPIPE, SIG_IGN);

  RetinaFace rf;
  const std::string model = opt.models + "/mnet.25-opt";
  if (!rf.load(model, opt.threads)) {
    std::fprintf(stderr, "m12d: cannot load model %s.{param,bin}\n", model.c_str());
    return 2;
  }
  int serial_fd = -1;
  if (opt.use_serial) {
    serial_fd = open_serial(opt.serial, 115200);
    if (serial_fd < 0) {
      std::fprintf(stderr, "m12d: cannot open serial %s\n", opt.serial.c_str());
      return 4;
    }
  }
  int listen_fd = -1;
  if (opt.tcp_port > 0) {
    listen_fd = open_tcp_listener(opt.tcp_bind, static_cast<uint16_t>(opt.tcp_port), 4);
    if (listen_fd < 0) {
      if (serial_fd >= 0) ::close(serial_fd);
      std::fprintf(stderr, "m12d: cannot listen on %s:%d\n", opt.tcp_bind.c_str(), opt.tcp_port);
      return 5;
    }
  }

  RuntimeCfg cfg;
  cfg.max_faces = opt.max_faces;
  cfg.fps_cap = opt.fps_cap;
  Shared sh;
  Snapshot snap;
  CaptureStats cap_stats;
  IoLoop* io = nullptr;

  auto hb_body_now = [&] {
    return hb_body(sh.temp, sh.fps, sh.paused ? 3 : 0, uptime_s(), mem_available_kb(), sh.det_frames,
                   cap_stats.reopens, io ? io->stats().dropped.load() : 0, cpu_cur_mhz());
  };
  StatusSource src;
  src.hb_body = hb_body_now;
  src.snap = [&]() -> std::string {
    std::lock_guard<std::mutex> lk(snap.mu);
    if (snap.frame.yuyv.empty() || !write_snap(snap.frame)) return "";
    return snap_members(snap.frame.id, kSnapPath, snap.faces, kScale, cfg.max_faces, cfg.with_lm);
  };
  src.version = kVersion;
  src.git = M12D_GIT;
  src.models = {"mnet.25-opt"};
  Dispatcher disp(cfg, src);
  io = new IoLoop(serial_fd, listen_fd, 2, [&](ChanId c, std::string_view line, bool too_long) {
    bool sub = io->subscribed(c);
    std::string r = disp.handle(line, too_long, sub);
    io->set_subscribed(c, sub);
    return r;
  });

  std::mutex out_mu;
  auto emit = [&](const std::string& line) {
    io->push(line);
    if (opt.use_stdout) {
      std::lock_guard<std::mutex> lk(out_mu);
      std::fputs(line.c_str(), stdout);
      std::fputc('\n', stdout);
      std::fflush(stdout);
    }
  };

  LatestFrame slot;
  V4L2Camera cam(opt.video, 640, 480, opt.cap_fps);
  std::thread cap_th([&] { capture_loop(cam, slot, g_stop, cap_stats); });
  std::thread io_th([&] { io->run(g_stop); });
  std::thread det_th([&] {
    std::vector<uint8_t> rgb(320 * 240 * 3);
    std::deque<int64_t> done;  // completion times within the last 2 s
    Frame f;
    uint64_t last = 0;
    while (!g_stop) {
      if (!slot.wait_newer(last, f, 1000)) {
        continue;
      }
      last = f.id;
      const int64_t t_now = now_ms();
      while (!done.empty() && t_now - done.front() > 2000) done.pop_front();
      sh.fps = done.size() / 2.0f;
      if (!cfg.det_on || sh.paused) continue;
      const auto t0 = Clock::now();
      yuyv_to_rgb_half(f.yuyv.data(), f.w, f.h, rgb.data());
      std::vector<FaceBox> faces = rf.detect(rgb.data(), f.w / 2, f.h / 2);
      emit(det_json(f.id, faces, kScale, cfg.max_faces, cfg.with_lm));
      sh.det_frames++;
      done.push_back(now_ms());
      {
        std::lock_guard<std::mutex> lk(snap.mu);
        std::swap(snap.frame, f);
        snap.faces.swap(faces);
      }
      const float cap = cfg.fps_cap;
      if (cap > 0) std::this_thread::sleep_until(t0 + std::chrono::microseconds(static_cast<int64_t>(1e6f / cap)));
    }
  });

  ThermalGuard guard;
  int64_t next_temp = 0, next_hb = now_ms() + opt.hb_ms;
  while (!g_stop) {
    const int64_t t = now_ms();
    if (t >= next_temp) {
      auto temp = read_temp_c();
      const bool p = guard.update(temp, t);
      if (p != sh.paused) std::fprintf(stderr, "m12d: detection %s (temp %d)\n", p ? "PAUSED" : "resumed", guard.last_temp());
      sh.paused = p;
      sh.temp = guard.last_temp();
      next_temp = t + 1000;
    }
    if (t >= next_hb) {
      emit("{\"hb\":" + hb_body_now() + "}");
      next_hb = t + opt.hb_ms;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::fprintf(stderr, "m12d: stopping\n");
  slot.stop();
  det_th.join();
  cap_th.join();
  io_th.join();
  delete io;
  return 0;
}
