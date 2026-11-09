#include "proto/commands.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include "proto/flat_json.h"

namespace {

std::string err(long long id, const char* code) {
  return "{\"id\":" + std::to_string(id) + ",\"ok\":false,\"err\":\"" + code + "\"}";
}

std::string json_str(const std::string& s) {
  std::string o = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      o += '\\';
      o += static_cast<char>(c);
    } else if (c < 0x20) {
      char b[8];
      std::snprintf(b, sizeof b, "\\u%04x", c);
      o += b;
    } else {
      o += static_cast<char>(c);
    }
  }
  return o + "\"";
}

bool only_keys(const std::map<std::string, FlatValue>& o, std::initializer_list<const char*> allowed) {
  for (const auto& kv : o) {
    bool ok = false;
    for (const char* a : allowed) ok = ok || kv.first == a;
    if (!ok) return false;
  }
  return true;
}

bool as_int(const FlatValue& v, long long lo, long long hi, long long& out) {
  const double* d = std::get_if<double>(&v);
  if (!d || !std::isfinite(*d) || std::floor(*d) != *d || *d < lo || *d > hi) return false;
  out = static_cast<long long>(*d);
  return true;
}

}  // namespace

Dispatcher::Dispatcher(RuntimeCfg& cfg, StatusSource src) : cfg_(cfg), src_(std::move(src)) {}

std::string Dispatcher::handle(std::string_view line, bool too_long, bool& sub) {
  if (too_long) return err(-1, "toolong");
  auto obj = parse_flat_json(line);
  if (!obj) return err(-1, "badjson");
  const auto& o = *obj;

  long long id;
  auto it = o.find("id");
  if (it == o.end() || !as_int(it->second, 0, 2147483647LL, id)) return err(-1, "badarg");
  auto dt = o.find("do");
  const std::string* cmd = dt == o.end() ? nullptr : std::get_if<std::string>(&dt->second);
  if (!cmd) return err(id, "unknown");
  const std::string head = "{\"id\":" + std::to_string(id) + ",\"ok\":true";

  if (*cmd == "ping" || *cmd == "ver" || *cmd == "status" || *cmd == "snap") {
    if (!only_keys(o, {"id", "do"})) return err(id, "badarg");
    if (*cmd == "ping") return head + "}";
    if (*cmd == "ver") {
      std::string s = head + ",\"ver\":" + json_str(src_.version) + ",\"git\":" + json_str(src_.git) + ",\"models\":[";
      for (size_t i = 0; i < src_.models.size(); i++) s += (i ? "," : "") + json_str(src_.models[i]);
      return s + "]}";
    }
    if (*cmd == "status") return head + ",\"hb\":" + src_.hb_body() + "}";
    std::string snap = src_.snap ? src_.snap() : std::string();
    if (snap.empty()) return err(id, "noface");
    return head + "," + snap + "}";
  }

  if (*cmd == "cfg") {
    if (!only_keys(o, {"id", "do", "det", "lm", "max_faces", "fps_cap", "sub"})) return err(id, "badarg");
    // Validate everything first; apply only if all arguments are valid.
    bool det = cfg_.det_on, lm = cfg_.with_lm, nsub = sub;
    long long mf = cfg_.max_faces;
    float fc = cfg_.fps_cap;
    for (const auto& kv : o) {
      const std::string& k = kv.first;
      const FlatValue& v = kv.second;
      if (k == "det" || k == "lm" || k == "sub") {
        const bool* b = std::get_if<bool>(&v);
        if (!b) return err(id, "badarg");
        (k == "det" ? det : k == "lm" ? lm : nsub) = *b;
      } else if (k == "max_faces") {
        if (!as_int(v, 1, 10, mf)) return err(id, "badarg");
      } else if (k == "fps_cap") {
        const double* d = std::get_if<double>(&v);
        if (!d || !std::isfinite(*d) || *d < 0 || *d > 30) return err(id, "badarg");
        fc = static_cast<float>(*d);
      }
    }
    cfg_.det_on = det;
    cfg_.with_lm = lm;
    cfg_.max_faces = static_cast<int>(mf);
    cfg_.fps_cap = fc;
    sub = nsub;
    char buf[160];
    std::snprintf(buf, sizeof buf, ",\"cfg\":{\"det\":%s,\"lm\":%s,\"max_faces\":%lld,\"fps_cap\":%.1f,\"sub\":%s}}",
                  det ? "true" : "false", lm ? "true" : "false", mf, fc, nsub ? "true" : "false");
    return head + buf;
  }

  return err(id, "unknown");
}
