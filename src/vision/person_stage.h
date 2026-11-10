#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "proto/det_json.h"
#include "vision/person_det.h"
#include "vision/person_tracker.h"
#include "vision/retinaface.h"

// Person detection for one detect-loop frame (phase 1b, user's option 1, 2026-11-10):
// 160x160 at prob >= 0.60; new (or due-for-recheck rejected) tracks are verified in the same
// frame by a full-frame 320x320 pass at prob >= 0.55. Returns confirmed persons paired with faces.
class PersonStage {
 public:
  static constexpr int kSize = 160, kVerifySize = 320;
  static constexpr float kProb = 0.60f, kVerifyProb = 0.55f;
  explicit PersonStage(PersonDetector& det) : det_(det) {}
  // rgb320: 320x240 RGB; ran_person: run the detector this frame (else report held tracks);
  // faces / face_ids: this frame's faces (prob-sorted, as RetinaFace returns them) and their
  // tracker ids; only the first max_faces faces -- the ones the det line outputs -- are paired.
  std::vector<PersonOut> step(const uint8_t* rgb320, bool ran_person, const std::vector<FaceBox>& faces,
                              const std::vector<int>& face_ids, int64_t now_ms, size_t max_faces = SIZE_MAX);
  void reset() { tracker_.clear(); }

 private:
  PersonDetector& det_;
  PersonTracker tracker_;
};
