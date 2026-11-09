#include "vision/align.h"

#include "mat.h"

namespace {
const float kTemplate112[10] = {38.2946f, 51.6963f, 73.5318f, 51.5014f, 56.0252f,
                                71.7366f, 41.5493f, 92.3655f, 70.7299f, 92.2041f};
}  // namespace

void similarity_to_template(const float lm[5][2], float tm[6]) {
  float from[10];
  for (int i = 0; i < 5; i++) {
    from[2 * i] = lm[i][0];
    from[2 * i + 1] = lm[i][1];
  }
  ncnn::get_affine_transform(from, kTemplate112, 5, tm);
}

void align_face(const uint8_t* rgb, int w, int h, const float lm[5][2], uint8_t* out112) {
  float tm[6], inv[6];
  similarity_to_template(lm, tm);
  ncnn::invert_affine_transform(tm, inv);  // warpaffine wants dst -> src
  ncnn::warpaffine_bilinear_c3(rgb, w, h, out112, 112, 112, inv);
}
