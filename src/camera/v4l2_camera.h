#pragma once
#include <string>
#include <vector>
#include "camera/camera.h"

// V4L2 mmap capture of YUYV frames (4 buffers).
class V4L2Camera : public ICamera {
 public:
  V4L2Camera(std::string dev, int w, int h, int fps);
  ~V4L2Camera() override;
  bool open() override;
  bool read(Frame& out, int timeout_ms) override;
  void close() override;

 private:
  struct Buf {
    void* p = nullptr;
    size_t len = 0;
  };
  std::string dev_;
  int w_, h_, fps_;
  int fd_ = -1;
  std::vector<Buf> bufs_;
  bool streaming_ = false;
};
