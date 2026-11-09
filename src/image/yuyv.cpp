#include "image/yuyv.h"

namespace {
inline uint8_t clamp8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : static_cast<uint8_t>(v)); }
}  // namespace

void yuyv_to_rgb_half(const uint8_t* yuyv, int w, int h, uint8_t* rgb_out) {
  // BT.601 limited-range coefficients scaled by 1024.
  constexpr int kY = 1192, kRV = 1634, kGU = 401, kGV = 833, kBU = 2065;
  constexpr int kRound = 1 << 19;
  const int stride = w * 2;
  for (int oy = 0; oy < h / 2; oy++) {
    const uint8_t* r0 = yuyv + (2 * oy) * stride;
    const uint8_t* r1 = r0 + stride;
    uint8_t* out = rgb_out + oy * (w / 2) * 3;
    for (int ox = 0; ox < w / 2; ox++) {
      // One output pixel = one macropixel (Y0 U Y1 V) from each of the two rows.
      const uint8_t* a = r0 + ox * 4;
      const uint8_t* b = r1 + ox * 4;
      const int ysum = a[0] + a[2] + b[0] + b[2];
      const int usum = a[1] + b[1];
      const int vsum = a[3] + b[3];
      // Values scaled by 1024: average luma = ysum/4, average chroma = sum/2.
      const int yv = ysum * 256 - 16 * 1024;
      const int u = usum * 512 - 128 * 1024;
      const int v = vsum * 512 - 128 * 1024;
      out[ox * 3 + 0] = clamp8((kY * yv + kRV * v + kRound) >> 20);
      out[ox * 3 + 1] = clamp8((kY * yv - kGU * u - kGV * v + kRound) >> 20);
      out[ox * 3 + 2] = clamp8((kY * yv + kBU * u + kRound) >> 20);
    }
  }
}
