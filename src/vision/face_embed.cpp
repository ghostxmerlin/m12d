#include "vision/face_embed.h"

#include <cmath>

#include "vision/stock_align.h"

float cosine(const Embedding& a, const Embedding& b) {
  float s = 0;
  for (size_t i = 0; i < a.size(); i++) s += a[i] * b[i];
  return s;
}

bool FaceEmbedder::load(const std::string& model_base, int num_threads) {
  net_.opt.num_threads = num_threads;
  loaded_ = net_.load_param((model_base + ".param").c_str()) == 0 && net_.load_model((model_base + ".bin").c_str()) == 0;
  return loaded_;
}

Embedding FaceEmbedder::embed(const uint8_t* rgb, int w, int h, const FaceBox& face) {
  Embedding e{};
  if (!loaded_) return e;
  // Stock preprocess() (arcface.cpp) with the stock helpers, but float landmarks: template x + 8
  // for 112 px, src laid out x0..x4 then y0..y4. The frame is RGB here (stock: BGR, then
  // bgr2rgb after the warp), so the model sees the same RGB crop.
  float dst[10] = {30.2946f, 65.5318f, 48.0252f, 33.5493f, 62.7299f,
                   51.6963f, 51.5014f, 71.7366f, 92.3655f, 92.2041f};
  for (int i = 0; i < 5; i++) dst[i] += 8.0f;
  float src[10], M[6];
  for (int i = 0; i < 5; i++) {
    src[i] = face.lm[i][0];
    src[i + 5] = face.lm[i][1];
  }
  stock::getAffineMatrix(src, dst, M);
  ncnn::Mat img = ncnn::Mat::from_pixels(rgb, ncnn::Mat::PIXEL_RGB, w, h), warped;
  stock::warpAffineMatrix(img, warped, M, 112, 112);
  ncnn::Mat in = stock::resize(warped, 112, 112);  // stock getFeature: also quantises to uint8
  ncnn::Extractor ex = net_.create_extractor();
  ex.set_light_mode(true);
  ex.input("data", in);
  ncnn::Mat out;
  ex.extract("fc1", out);
  if (out.total() < e.size()) return e;
  double n = 0;
  for (size_t i = 0; i < e.size(); i++) {
    e[i] = out[i];
    n += double(e[i]) * e[i];
  }
  const float inv = n > 0 ? static_cast<float>(1.0 / std::sqrt(n)) : 0.f;
  for (float& v : e) v *= inv;
  return e;
}
