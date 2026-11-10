#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "net.h"

// One detected person. Coordinates in the input image space (320x240 in m12d).
struct PersonBox {
  float x, y, w, h, prob;
};

// rows: n rows of Yolov3DetectionOutput, each [label, prob, x0, y0, x1, y1] (normalised).
std::vector<PersonBox> decode_person_rows(const float* rows, int n, int img_w, int img_h, float prob_th);

// Stock yolo_20class Yolo-Fastest (VOC; label 15 = person). Its param bakes a 0.55 threshold.
class PersonDetector {
 public:
  static constexpr int kLabelPerson = 15;
  bool load(const std::string& model_base, int num_threads);
  // rgb: packed RGB w x h, stretched to size x size (stock: no letterbox, /255). Desc by prob.
  std::vector<PersonBox> detect(const uint8_t* rgb, int w, int h, int size, float prob_th);

 private:
  ncnn::Net net_;
  bool loaded_ = false;
};
