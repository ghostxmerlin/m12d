#pragma once
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "vision/face_embed.h"

struct Match {
  std::string name;  // "" = nobody at or above the threshold
  float sim;         // best similarity found (0 for an empty library)
};
enum class AddResult { kOk, kFull, kConflict, kBadName };

// Face library (design §6): up to kMaxNames names, each with up to kMaxSamples L2-normalised
// samples (oldest dropped). Only the detect thread uses it, so it is not thread-safe.
// File: "M12F", u32 version 1, u32 names; per name u32 len, bytes, u32 samples, 128 x f32 each;
// trailing CRC32 (0xEDB88320) of everything before it. Little-endian.
class FaceDb {
 public:
  static constexpr size_t kMaxNames = 16, kMaxSamples = 20, kMaxNameBytes = 32;
  static bool valid_name(const std::string& n);
  // kConflict (and *conflict_name) when any sample scores >= th against ANOTHER name; nothing stored.
  AddResult add(const std::string& name, const std::vector<Embedding>& samples, float th, std::string* conflict_name);
  Match best(const Embedding& e, float th) const;
  bool forget(const std::string& name);
  std::vector<std::pair<std::string, size_t>> list() const;
  std::string serialize() const;
  static std::optional<FaceDb> parse(const std::string& bytes);
  bool save(const std::string& path) const;  // tmp + fsync + rename + dir fsync
  // Missing file -> empty, note "". Corrupt -> renamed to <path>.bad.<unix_s>, empty, note says so.
  static FaceDb load(const std::string& path, std::string* note);

 private:
  struct Person {
    std::string name;
    std::deque<Embedding> samples;
  };
  std::vector<Person> people_;
};
