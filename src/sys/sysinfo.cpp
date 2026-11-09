#include "sys/sysinfo.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {
std::string slurp(const char* path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
}  // namespace

long parse_mem_available_kb(const std::string& meminfo) {
  const std::string key = "MemAvailable:";
  size_t p = meminfo.find(key);
  if (p == std::string::npos) return -1;
  return std::strtol(meminfo.c_str() + p + key.size(), nullptr, 10);
}

int64_t parse_uptime_s(const std::string& uptime) {
  char* end = nullptr;
  double v = std::strtod(uptime.c_str(), &end);
  if (end == uptime.c_str()) return -1;
  return static_cast<int64_t>(v);
}

float parse_cpu_mhz(const std::string& s) {
  char* end = nullptr;
  long khz = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str()) return 0.f;
  return khz / 1000.0f;
}

long mem_available_kb() { return parse_mem_available_kb(slurp("/proc/meminfo")); }
int64_t uptime_s() { return parse_uptime_s(slurp("/proc/uptime")); }
float cpu_cur_mhz() { return parse_cpu_mhz(slurp("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq")); }
