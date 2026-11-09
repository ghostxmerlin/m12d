#pragma once
#include <cstdint>
#include <optional>
#include <string>

// "Temperature 74\n" -> 74; anything else -> nullopt.
std::optional<int> parse_temp_r(const std::string& s);
std::optional<int> read_temp_c(const std::string& path = "/sys/class/mstar/msys/TEMP_R");

// Detection pause guard: pause at >= pause_at, resume below resume_below (hysteresis).
// No valid reading for unreadable_ms counts as overheated (fail safe) until a valid
// reading arrives.
class ThermalGuard {
 public:
  ThermalGuard(int pause_at = 90, int resume_below = 87, int unreadable_ms = 3000);
  bool update(std::optional<int> temp, int64_t now_ms);  // true = pause detection
  int last_temp() const { return last_temp_; }            // -1 if never read

 private:
  int pause_at_, resume_below_, unreadable_ms_;
  int last_temp_ = -1;
  bool hot_ = false, unreadable_ = false, have_ref_ = false;
  int64_t last_valid_ms_ = 0;
};
