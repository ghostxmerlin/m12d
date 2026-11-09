#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "net.h"

struct AgeGenderResult {
  int age = -1;
  int gender = -1;  // raw argmax of fc1[0..1]; label meaning verified in results/*attributes*
};

// fc1 (202 floats): gender = argmax(fc1[0..1]); age = number of the 100 (no, yes) pairs
// in fc1[2..201] whose "yes" wins.
AgeGenderResult decode_age_gender(const float* fc1);

// deepinx/age-gender-estimation model converted to ncnn: input "data" RGB 112x112,
// normalisation is inside the graph; output "fc1".
class AgeGender {
 public:
  bool load(const std::string& model_base, int num_threads);
  AgeGenderResult infer(const uint8_t* rgb112, std::vector<float>* fc1_out = nullptr);

 private:
  ncnn::Net net_;
  bool loaded_ = false;
};
