#pragma once
#include <optional>
#include <string>

struct Options {
  std::string models = "models", serial = "/dev/ttyS1", video = "/dev/video0";
  bool use_serial = true, use_stdout = false;
  bool use_attr = true;  // gender / age-group sampling (models/genderage)
  bool use_person = true;  // person detection (models/yolo-fastest), phase 1b
  bool use_faceid = true;  // on-demand face ID (models/mobilefacenet), phase 2
  std::string data = "data";  // persistent data dir (face library data/faces.bin)
  int tcp_port = 5555;
  std::string tcp_bind = "10.254.239.1";  // USB-network address; never the WiFi STA LAN
  float fps_cap = 0;
  int cap_fps = 30;  // detection at 192x144 runs >10 fps, so the camera must deliver more
  int cap_w = 320, cap_h = 240;  // 320x240 (default) or 640x480; frames are brought to 320x240 RGB first
  int det_w = 192, det_h = 144;  // detector input (near mode); 320x240 = stock-identical, see results/*sweep*
  int max_faces = 10;
  int threads = 2;
  int hb_ms = 2000;
};

// --models DIR --serial PATH --no-serial --no-attr --no-person --no-faceid --data DIR --tcp-port N --tcp-bind ADDR --stdout --fps-cap F
// --cap-fps N --cap-size 320x240|640x480 --det-size WxH --max-faces N --threads N --hb-ms N
std::optional<Options> parse_args(int argc, char** argv, std::string& err);
