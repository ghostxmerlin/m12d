#include "doctest.h"
#include "vision/det_schedule.h"

TEST_CASE("with a recent face: face every frame, person every 5th") {
  for (uint64_t n = 0; n < 10; n++) {
    DetPlan p = plan_frame(n, true);
    CHECK(p.face);
    CHECK(p.person == (n % 5 == 0));
  }
}

TEST_CASE("without a face: face and person alternate") {
  for (uint64_t n = 0; n < 10; n++) {
    DetPlan p = plan_frame(n, false);
    CHECK(p.face == (n % 2 == 0));
    CHECK(p.person == (n % 2 == 1));
  }
}
