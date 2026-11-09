#include "camera/v4l2_camera.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>

namespace {
int xioctl(int fd, unsigned long req, void* arg) {
  int r;
  do {
    r = ioctl(fd, req, arg);
  } while (r == -1 && errno == EINTR);
  return r;
}
int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

V4L2Camera::V4L2Camera(std::string dev, int w, int h, int fps)
    : dev_(std::move(dev)), w_(w), h_(h), fps_(fps) {}

V4L2Camera::~V4L2Camera() { close(); }

bool V4L2Camera::open() {
  close();
  fd_ = ::open(dev_.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0) {
    std::fprintf(stderr, "v4l2: open %s: %s\n", dev_.c_str(), std::strerror(errno));
    return false;
  }
  v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = w_;
  fmt.fmt.pix.height = h_;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (xioctl(fd_, VIDIOC_S_FMT, &fmt) < 0 || fmt.fmt.pix.width != static_cast<unsigned>(w_) ||
      fmt.fmt.pix.height != static_cast<unsigned>(h_) || fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
    std::fprintf(stderr, "v4l2: S_FMT YUYV %dx%d rejected (got %ux%u)\n", w_, h_, fmt.fmt.pix.width,
                 fmt.fmt.pix.height);
    close();
    return false;
  }
  v4l2_streamparm parm{};
  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  parm.parm.capture.timeperframe.numerator = 1;
  parm.parm.capture.timeperframe.denominator = fps_;
  xioctl(fd_, VIDIOC_S_PARM, &parm);  // best effort; the driver may round

  v4l2_requestbuffers req{};
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
    std::fprintf(stderr, "v4l2: REQBUFS: %s\n", std::strerror(errno));
    close();
    return false;
  }
  bufs_.resize(req.count);
  for (unsigned i = 0; i < req.count; i++) {
    v4l2_buffer b{};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = i;
    if (xioctl(fd_, VIDIOC_QUERYBUF, &b) < 0) {
      close();
      return false;
    }
    void* p = mmap(nullptr, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, b.m.offset);
    if (p == MAP_FAILED) {
      close();
      return false;
    }
    bufs_[i] = {p, b.length};
    if (xioctl(fd_, VIDIOC_QBUF, &b) < 0) {
      close();
      return false;
    }
  }
  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
    std::fprintf(stderr, "v4l2: STREAMON: %s\n", std::strerror(errno));
    close();
    return false;
  }
  streaming_ = true;
  return true;
}

bool V4L2Camera::read(Frame& out, int timeout_ms) {
  if (fd_ < 0) return false;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd_, &fds);
  timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
  int r = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
  if (r <= 0) return false;  // timeout or error
  v4l2_buffer b{};
  b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  b.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd_, VIDIOC_DQBUF, &b) < 0) return false;
  const size_t need = static_cast<size_t>(w_) * h_ * 2;
  bool ok = b.index < bufs_.size() && b.bytesused >= need;
  if (ok) {
    out.yuyv.assign(static_cast<uint8_t*>(bufs_[b.index].p), static_cast<uint8_t*>(bufs_[b.index].p) + need);
    out.w = w_;
    out.h = h_;
    out.ts_ms = now_ms();
  }
  xioctl(fd_, VIDIOC_QBUF, &b);
  return ok;
}

void V4L2Camera::close() {
  if (fd_ < 0) return;
  if (streaming_) {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd_, VIDIOC_STREAMOFF, &type);
    streaming_ = false;
  }
  for (auto& b : bufs_)
    if (b.p) munmap(b.p, b.len);
  bufs_.clear();
  ::close(fd_);
  fd_ = -1;
}
