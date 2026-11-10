#pragma once
#include <cstdint>
#include <vector>
#include "vision/person_det.h"

struct PersonTrack {
  enum State { kPending, kConfirmed, kRejected };
  int id;
  PersonBox box;
  State state;
};

// IoU tracker for 160x160 person detections. A new track is pending until a full-frame
// 320x320 pass verifies it (confirmed) or does not (rejected, e.g. a coat on a chair).
// Rejected tracks are re-verified every kRecheckMs so a real person standing in front of
// a rejected object is picked up. Only confirmed tracks are reported.
class PersonTracker {
 public:
  static constexpr float kMatchIou = 0.3f;
  static constexpr int64_t kForgetMs = 1500, kRecheckMs = 5000;
  void update(const std::vector<PersonBox>& dets160, int64_t now_ms);
  bool needs_verify(int64_t now_ms) const;
  void apply_verify(const std::vector<PersonBox>& dets320, int64_t now_ms);
  std::vector<PersonTrack> confirmed() const;
  void clear();

 private:
  struct Track {
    PersonTrack t;
    int64_t last_seen_ms;
    int64_t verified_ms;
  };
  std::vector<Track> tracks_;
  int next_id_ = 1;
};
