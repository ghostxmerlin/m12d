#pragma once
#include <array>
#include <cstdint>
#include <string>
#include "net.h"
#include "vision/retinaface.h"

// 128-d face feature, L2-normalised. Cosine similarity = dot product.
using Embedding = std::array<float, 128>;
float cosine(const Embedding& a, const Embedding& b);

// Stock mobilefacenet (UnitV2Framework src/arcface.cpp): stock five-point alignment (stock_align) to 112x112,
// input "data" in RGB order (stock warps a BGR frame, then bgr2rgb), raw 0..255, output "fc1",
// L2-normalised.
class FaceEmbedder {
 public:
  bool load(const std::string& model_base, int num_threads);
  // rgb: packed RGB w x h; face (box + 5 landmarks) in the same coordinates.
  Embedding embed(const uint8_t* rgb, int w, int h, const FaceBox& face);

 private:
  ncnn::Net net_;
  bool loaded_ = false;
};
