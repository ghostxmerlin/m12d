#pragma once
// Stock UnitV2Framework (@0e48be6) face-alignment helpers, used by FaceEmbedder so the
// recognition crop matches the stock face_recognition path exactly (phase-2 A layer).
// Portions copyright M5Stack (UnitV2Framework), Apache License 2.0. See THIRD_PARTY.md.
#include "mat.h"

namespace stock {
ncnn::Mat resize(ncnn::Mat src, int w, int h);
void getAffineMatrix(float* src_5pts, const float* dst_5pts, float* M);
void warpAffineMatrix(ncnn::Mat src, ncnn::Mat &dst, float *M, int dst_w, int dst_h);
}  // namespace stock
