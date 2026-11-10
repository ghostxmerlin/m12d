#include "app/options.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace {
bool to_int(const char* s, long lo, long hi, int& out) {
  char* end = nullptr;
  errno = 0;
  long v = std::strtol(s, &end, 10);
  if (errno || end == s || *end || v < lo || v > hi) return false;
  out = static_cast<int>(v);
  return true;
}
bool to_float(const char* s, float lo, float hi, float& out) {
  char* end = nullptr;
  errno = 0;
  float v = std::strtof(s, &end);
  if (errno || end == s || *end || !(v >= lo && v <= hi)) return false;
  out = v;
  return true;
}
}  // namespace

std::optional<Options> parse_args(int argc, char** argv, std::string& err) {
  Options o;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--no-serial") {
      o.use_serial = false;
      continue;
    }
    if (a == "--no-attr") {
      o.use_attr = false;
      continue;
    }
    if (a == "--no-person") {
      o.use_person = false;
      continue;
    }
    if (a == "--no-faceid") {
      o.use_faceid = false;
      continue;
    }
    if (a == "--stdout") {
      o.use_stdout = true;
      continue;
    }
    if (i + 1 >= argc) {
      err = "missing value for " + a;
      return std::nullopt;
    }
    const char* v = argv[++i];
    bool ok = true;
    if (a == "--models") o.models = v;
    else if (a == "--data") o.data = v;
    else if (a == "--serial") o.serial = v;
    else if (a == "--tcp-bind") o.tcp_bind = v;
    else if (a == "--tcp-port") ok = to_int(v, 0, 65535, o.tcp_port);
    else if (a == "--fps-cap") ok = to_float(v, 0, 30, o.fps_cap) && (o.fps_cap == 0 || o.fps_cap >= 0.5f);
    else if (a == "--cap-fps") ok = to_int(v, 1, 30, o.cap_fps);
    else if (a == "--cap-size") {
      const std::string sz = v;
      ok = sz == "320x240" || sz == "640x480";
      if (ok) {
        o.cap_w = sz == "320x240" ? 320 : 640;
        o.cap_h = sz == "320x240" ? 240 : 480;
      }
    }
    else if (a == "--det-size") {
      // 4:3 sizes measured in results/2026-11-09-input-size-sweep.md
      const std::string sz = v;
      const char* ok_sizes[] = {"320x240", "256x192", "224x168", "192x144", "160x120"};
      ok = false;
      for (const char* k : ok_sizes) ok = ok || sz == k;
      if (ok) std::sscanf(v, "%dx%d", &o.det_w, &o.det_h);
    }
    else if (a == "--max-faces") ok = to_int(v, 1, 10, o.max_faces);
    else if (a == "--threads") ok = to_int(v, 1, 4, o.threads);
    else if (a == "--hb-ms") ok = to_int(v, 100, 60000, o.hb_ms);
    else {
      err = "unknown option " + a;
      return std::nullopt;
    }
    if (!ok) {
      err = "bad value for " + a + ": " + v;
      return std::nullopt;
    }
  }
  return o;
}
