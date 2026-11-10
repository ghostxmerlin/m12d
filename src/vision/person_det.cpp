#include "vision/person_det.h"

#include <algorithm>

std::vector<PersonBox> decode_person_rows(const float* rows, int n, int img_w, int img_h, float prob_th) {
  std::vector<PersonBox> out;
  for (int i = 0; i < n; i++) {
    const float* v = rows + i * 6;
    if (static_cast<int>(v[0]) != PersonDetector::kLabelPerson || v[1] < prob_th) continue;
    out.push_back(PersonBox{v[2] * img_w, v[3] * img_h, (v[4] - v[2]) * img_w, (v[5] - v[3]) * img_h, v[1]});
  }
  std::stable_sort(out.begin(), out.end(), [](const PersonBox& a, const PersonBox& b) { return a.prob > b.prob; });
  return out;
}

bool PersonDetector::load(const std::string& model_base, int num_threads) {
  net_.opt.num_threads = num_threads;
  net_.opt.use_vulkan_compute = false;
  loaded_ = net_.load_param((model_base + ".param").c_str()) == 0 && net_.load_model((model_base + ".bin").c_str()) == 0;
  return loaded_;
}

std::vector<PersonBox> PersonDetector::detect(const uint8_t* rgb, int w, int h, int size, float prob_th) {
  if (!loaded_) return {};
  ncnn::Mat in = ncnn::Mat::from_pixels_resize(rgb, ncnn::Mat::PIXEL_RGB, w, h, size, size);
  const float mean[3] = {0.f, 0.f, 0.f}, norm[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
  in.substract_mean_normalize(mean, norm);
  ncnn::Extractor ex = net_.create_extractor();
  ex.input("data", in);
  ncnn::Mat out;
  ex.extract("output", out);
  // Rows are contiguous 6-float records (w == 6); an empty result is a 0-row Mat.
  if (out.empty() || out.w != 6) return {};
  std::vector<float> rows(static_cast<size_t>(out.h) * 6);
  for (int r = 0; r < out.h; r++) std::copy(out.row(r), out.row(r) + 6, rows.begin() + r * 6);
  return decode_person_rows(rows.data(), out.h, w, h, prob_th);
}
