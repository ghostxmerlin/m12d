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

// Tracker for 160x160 person detections. A new track is pending until a full-frame 320x320
// pass verifies it (confirmed) or does not (rejected, e.g. a coat on a chair).
// - Every 320 pass re-verifies every track; confirmed and rejected tracks also become due for
//   a recheck kRecheckMs after their last verify (a person who left a chair no longer keeps a
//   coat "confirmed"; a person in front of a rejected coat gets picked up).
// - Matching: IoU >= kMatchIou, or centers within kCenterGate x box height with similar
//   heights (a walking person moves ~0.7 box widths between person passes at 15 fps).
// - Forgetting counts missed person passes, not wall time (fps_cap / throttling).
//   Only tracks seen in the last kReportMissed+1 passes are reported.
class PersonTracker {
 public:
  static constexpr float kMatchIou = 0.3f, kCenterGate = 0.5f;
  static constexpr int kForgetPasses = 4, kReportMissed = 1;
  static constexpr int64_t kRecheckMs = 5000;
  void update(const std::vector<PersonBox>& dets160, int64_t now_ms);
  bool needs_verify(int64_t now_ms) const;
  void apply_verify(const std::vector<PersonBox>& dets320, int64_t now_ms);
  std::vector<PersonTrack> confirmed() const;
  void clear();

 private:
  struct Track {
    PersonTrack t;
    int missed;
    int64_t verified_ms;
  };
  std::vector<Track> tracks_;
  int next_id_ = 1;
};
