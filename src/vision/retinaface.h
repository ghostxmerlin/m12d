#pragma once
#include <string>
#include <vector>
#include "net.h"

// One detected face. Coordinates are in the detector input space (e.g. 320x240).
struct FaceBox {
  float x, y, w, h, prob;
  float lm[5][2];
};

// RetinaFace mnet.25 detector, post-processing ported from the stock
// UnitV2Framework main/face_detector.cpp (commit 0e48be6).
class RetinaFace {
 public:
  // Returns false if either .param or .bin fails to load.
  bool load(const std::string& model_base, int num_threads);
  // rgb: packed RGB, w x h. Results sorted by descending prob after NMS.
  std::vector<FaceBox> detect(const uint8_t* rgb, int w, int h, float prob_th = 0.8f, float nms_th = 0.4f);

 private:
  ncnn::Net net_;
  bool loaded_ = false;
};

// Multiply every coordinate (box and landmarks) by k, e.g. to map 192x144 results to 320x240.
void scale_faces(std::vector<FaceBox>& faces, float k);

// Bilinear resize of packed RGB (ncnn::resize_bilinear_c3).
void resize_rgb(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh);
