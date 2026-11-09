#pragma once
#include <cstdint>
#include <string>
#include "net.h"
#include "vision/retinaface.h"

// insightface buffalo_s genderage.onnx, converted with ncnn 6afc736 onnx2ncnn.
// Input "data": 96x96 RGB, normalisation inside the graph. Output "fc1": 3 floats =
// gender logits (index 1 = male, confirmed by the user on 2026-11-09) and age / 100.
struct GenderAgeResult {
  int gender = -1;      // 0 female, 1 male
  int age = -1;         // years
  float margin = 0.f;   // |logit1 - logit0|: small = unsure
};

GenderAgeResult decode_genderage(const float fc1[3]);

// Age groups for StackChan: child <= 12, young 13-29, adult 30-54, elder >= 55.
const char* age_group(int age);

// insightface Attribute crop: no rotation; the box center goes to (48,48) and the long
// side * 1.5 spans 96 px. tm maps image -> 96x96 crop.
void genderage_crop_transform(const FaceBox& box, float tm[6]);
// Bilinear crop with a black border (cv2.warpAffine borderValue=0 in insightface).
void genderage_crop(const uint8_t* rgb, int w, int h, const FaceBox& box, uint8_t* out96);

class GenderAge {
 public:
  bool load(const std::string& model_base, int num_threads);
  GenderAgeResult infer(const uint8_t* rgb96, float raw_out[3] = nullptr);

 private:
  ncnn::Net net_;
  bool loaded_ = false;
};
