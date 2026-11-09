#include "vision/age_gender.h"

AgeGenderResult decode_age_gender(const float* fc1) {
  AgeGenderResult r;
  r.gender = fc1[1] > fc1[0] ? 1 : 0;
  int age = 0;
  for (int k = 0; k < 100; k++)
    if (fc1[2 + 2 * k + 1] > fc1[2 + 2 * k]) age++;
  r.age = age;
  return r;
}

bool AgeGender::load(const std::string& model_base, int num_threads) {
  net_.opt.lightmode = true;
  net_.opt.openmp_blocktime = 0;
  net_.opt.num_threads = num_threads;
  loaded_ = net_.load_param((model_base + ".param").c_str()) == 0 &&
            net_.load_model((model_base + ".bin").c_str()) == 0;
  return loaded_;
}

AgeGenderResult AgeGender::infer(const uint8_t* rgb112, std::vector<float>* fc1_out) {
  AgeGenderResult r;
  if (!loaded_) return r;
  ncnn::Mat in = ncnn::Mat::from_pixels(rgb112, ncnn::Mat::PIXEL_RGB, 112, 112);
  ncnn::Extractor ex = net_.create_extractor();
  ex.input("data", in);
  ncnn::Mat out;
  if (ex.extract("fc1", out) != 0 || out.total() != 202) return r;
  const float* p = static_cast<const float*>(out.data);
  if (fc1_out) fc1_out->assign(p, p + 202);
  return decode_age_gender(p);
}
