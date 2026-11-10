#pragma once
#include <cstdint>

struct DetPlan {
  bool face, person;
};
// n: detection-loop counter from 0; face_recent: a face was detected <= 1000 ms ago.
// Face present: face every frame, person every 5th. No face: face and person alternate.
DetPlan plan_frame(uint64_t n, bool face_recent);
