#pragma once
#include <atomic>
#include <cstdint>
#include "camera/camera.h"
#include "core/latest_frame.h"

struct CaptureStats {
  std::atomic<uint64_t> frames{0}, reopens{0}, errors{0};
};

// Reads frames into slot until stop. Three consecutive read failures close the camera,
// wait reopen_backoff_ms, and reopen; failed opens are retried with the same backoff.
// Backoff sleeps are interruptible, so stop is honoured within ~read_timeout_ms.
void capture_loop(ICamera& cam, LatestFrame& slot, std::atomic<bool>& stop, CaptureStats& st,
                  int read_timeout_ms = 1000, int reopen_backoff_ms = 500);
