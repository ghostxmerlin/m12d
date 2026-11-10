#include "vision/det_schedule.h"

DetPlan plan_frame(uint64_t n, bool face_recent) {
  if (face_recent) return DetPlan{true, n % 5 == 0};
  const bool even = n % 2 == 0;
  return DetPlan{even, !even};
}
