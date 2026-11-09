#pragma once
#include "core/latest_frame.h"

class ICamera {
 public:
  virtual ~ICamera() = default;
  virtual bool open() = 0;                              // open device and start streaming
  virtual bool read(Frame& out, int timeout_ms) = 0;    // true with a frame; false on timeout/error
  virtual void close() = 0;
};
