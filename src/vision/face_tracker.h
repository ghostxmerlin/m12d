#pragma once
#include <cstdint>
#include <deque>
#include <vector>
#include "vision/genderage.h"
#include "vision/retinaface.h"

struct FaceAttr {
  bool have = false;
  int gender = -1;  // 1 male, 0 female, -1 unsure
  int age = -1;     // median of recent samples
};

// Minimal IoU tracker that also schedules gender/age sampling per person:
// a new face gets 3 samples >= 200 ms apart, then one refresh every 5 s.
class FaceTracker {
 public:
  static constexpr float kMatchIou = 0.3f;
  static constexpr int64_t kForgetMs = 1000, kSampleGapMs = 200, kRefreshMs = 5000;
  static constexpr int kFirstSamples = 3, kKeepSamples = 5;
  static constexpr float kMinMargin = 1.0f;  // gender votes below this margin are ignored

  // Returns a stable id per face (same order as faces).
  std::vector<int> update(const std::vector<FaceBox>& faces, int64_t now_ms);
  // Index (into ids) of the face that should get an attribute sample now, or -1.
  int pick_for_attr(const std::vector<int>& ids, int64_t now_ms) const;
  void add_sample(int id, const GenderAgeResult& r, int64_t now_ms);
  FaceAttr attr(int id) const;

 private:
  struct Track {
    int id;
    FaceBox box;
    int64_t last_seen_ms;
    int64_t last_sample_ms = INT64_MIN / 2;
    int samples = 0;
    std::deque<GenderAgeResult> recent;
  };
  const Track* find(int id) const;
  std::vector<Track> tracks_;
  int next_id_ = 1;
};
