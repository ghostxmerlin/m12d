#include "proto/det_json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

std::string det_json(uint64_t frame_id, const std::vector<FaceBox>& faces, float scale, size_t max_faces,
                     bool with_lm, const std::vector<FaceExtra>* extra) {
  std::vector<size_t> order(faces.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return faces[a].prob > faces[b].prob; });
  if (order.size() > max_faces) order.resize(max_faces);

  char buf[256];
  std::string s;
  s.reserve(64 + order.size() * 160);
  std::snprintf(buf, sizeof buf, "{\"f\":%llu,\"num\":%zu,\"face\":[", static_cast<unsigned long long>(frame_id),
                order.size());
  s += buf;
  for (size_t n = 0; n < order.size(); n++) {
    const FaceBox& b = faces[order[n]];
    std::snprintf(buf, sizeof buf, "%s{\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f,\"prob\":%.3f", n ? "," : "",
                  b.x * scale, b.y * scale, b.w * scale, b.h * scale, b.prob);
    s += buf;
    if (with_lm) {
      s += ",\"mark\":[";
      for (int k = 0; k < 5; k++) {
        // stock: (int)landmark.x * IMAGE_DIV  -> truncate first, then scale
        std::snprintf(buf, sizeof buf, "%s{\"x\":%d,\"y\":%d}", k ? "," : "",
                      static_cast<int>(static_cast<int>(b.lm[k][0]) * scale),
                      static_cast<int>(static_cast<int>(b.lm[k][1]) * scale));
        s += buf;
      }
      s += "]";
    }
    if (extra && order[n] < extra->size()) {
      const FaceExtra& e = (*extra)[order[n]];
      std::snprintf(buf, sizeof buf, ",\"id\":%d", e.id);
      s += buf;
      if (e.attr.have) {
        const char* g = e.attr.gender == 1 ? "m" : e.attr.gender == 0 ? "f" : "?";
        std::snprintf(buf, sizeof buf, ",\"gender\":\"%s\",\"age\":%d,\"age_group\":\"%s\"", g, e.attr.age,
                      age_group(e.attr.age));
        s += buf;
      }
    }
    s += "}";
  }
  s += "],\"running\":\"Face Detector\"}";
  return s;
}

std::string hb_body(int temp_c, float fps, int tier, int64_t uptime_s, long mem_avail_kb, uint64_t frames,
                    uint64_t reopens, uint64_t dropped_lines, float cpu_mhz) {
  char buf[256];
  std::snprintf(buf, sizeof buf,
                "{\"t\":%d,\"fps\":%.1f,\"tier\":%d,\"up\":%lld,\"mem\":%ld,\"frames\":%llu,\"reopens\":%llu,"
                "\"drop\":%llu,\"mhz\":%ld}",
                temp_c, fps, tier, static_cast<long long>(uptime_s), mem_avail_kb,
                static_cast<unsigned long long>(frames), static_cast<unsigned long long>(reopens),
                static_cast<unsigned long long>(dropped_lines), std::lround(cpu_mhz));
  return buf;
}

std::string hb_json(int temp_c, float fps, int tier, int64_t uptime_s, long mem_avail_kb, uint64_t frames,
                    uint64_t reopens, uint64_t dropped_lines, float cpu_mhz) {
  return "{\"hb\":" + hb_body(temp_c, fps, tier, uptime_s, mem_avail_kb, frames, reopens, dropped_lines, cpu_mhz) +
         "}";
}

std::string snap_members(uint64_t frame_id, const std::string& path, const std::vector<FaceBox>& faces, float scale,
                         size_t max_faces, bool with_lm) {
  // Reuse det_json's face formatting: take its "num":..,"face":[...] span.
  const std::string d = det_json(frame_id, faces, scale, max_faces, with_lm, nullptr);
  const size_t a = d.find("\"num\":");
  const size_t b = d.rfind(",\"running\":");
  return "\"f\":" + std::to_string(frame_id) + ",\"path\":\"" + path + "\"," + d.substr(a, b - a);
}
