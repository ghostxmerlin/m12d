#include "vision/genderage.h"

#include <algorithm>
#include <cmath>
#include "mat.h"

GenderAgeResult decode_genderage(const float fc1[3]) {
  GenderAgeResult r;
  r.gender = fc1[1] > fc1[0] ? 1 : 0;
  r.margin = std::fabs(fc1[1] - fc1[0]);
  r.age = static_cast<int>(std::lround(fc1[2] * 100.0f));
  return r;
}

const char* age_group(int age) {
  if (age <= 12) return "child";
  if (age <= 29) return "young";
  if (age <= 54) return "adult";
  return "elder";
}

void genderage_crop_transform(const FaceBox& b, float tm[6]) {
  const float s = 96.0f / (std::max(b.w, b.h) * 1.5f);
  const float cx = b.x + b.w * 0.5f, cy = b.y + b.h * 0.5f;
  tm[0] = s;
  tm[1] = 0;
  tm[2] = 48.0f - s * cx;
  tm[3] = 0;
  tm[4] = s;
  tm[5] = 48.0f - s * cy;
}

void genderage_crop(const uint8_t* rgb, int w, int h, const FaceBox& box, uint8_t* out96) {
  float tm[6], inv[6];
  genderage_crop_transform(box, tm);
  ncnn::invert_affine_transform(tm, inv);
  ncnn::warpaffine_bilinear_c3(rgb, w, h, out96, 96, 96, inv, 0, 0);
}

bool GenderAge::load(const std::string& model_base, int num_threads) {
  net_.opt.lightmode = true;
  net_.opt.openmp_blocktime = 0;
  net_.opt.num_threads = num_threads;
  loaded_ = net_.load_param((model_base + ".param").c_str()) == 0 &&
            net_.load_model((model_base + ".bin").c_str()) == 0;
  return loaded_;
}

GenderAgeResult GenderAge::infer(const uint8_t* rgb96, float raw_out[3]) {
  GenderAgeResult r;
  if (!loaded_) return r;
  ncnn::Mat in = ncnn::Mat::from_pixels(rgb96, ncnn::Mat::PIXEL_RGB, 96, 96);
  ncnn::Extractor ex = net_.create_extractor();
  ex.input("data", in);
  ncnn::Mat out;
  if (ex.extract("fc1", out) != 0 || out.total() != 3) return r;
  const float* p = static_cast<const float*>(out.data);
  if (raw_out) std::copy(p, p + 3, raw_out);
  return decode_genderage(p);
}
