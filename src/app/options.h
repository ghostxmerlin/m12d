#pragma once
#include <optional>
#include <string>

struct Options {
  std::string models = "models", serial = "/dev/ttyS1", video = "/dev/video0";
  bool use_serial = true, use_stdout = false;
  int tcp_port = 5555;
  std::string tcp_bind = "10.254.239.1";  // USB-network address; never the WiFi STA LAN
  float fps_cap = 0;
  int cap_fps = 10;  // 320x240@10 measured best: 7.36 det fps vs 7.17 @15, 7.14 @30
  int cap_w = 320, cap_h = 240;  // 320x240 (default) or 640x480; detector input is always 320x240
  int max_faces = 10;
  int threads = 2;
  int hb_ms = 2000;
};

// --models DIR --serial PATH --no-serial --tcp-port N --tcp-bind ADDR --stdout --fps-cap F
// --cap-fps N --cap-size 320x240|640x480 --max-faces N --threads N --hb-ms N
std::optional<Options> parse_args(int argc, char** argv, std::string& err);
