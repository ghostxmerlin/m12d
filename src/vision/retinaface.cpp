// Portions copyright M5Stack (UnitV2Framework), licensed under the Apache License 2.0
// (http://www.apache.org/licenses/LICENSE-2.0). See THIRD_PARTY.md.
// Changes: OpenCV types replaced by plain structs, omp sections removed, RGB input.
// Post-processing ported from UnitV2Framework main/face_detector.cpp @0e48be6
// (qsort_descent_inplace, nms_sorted_bboxes, generate_anchors, generate_proposals,
// detect_retinaface). Arithmetic is kept identical; cv::Rect_<float> is replaced by
// an equivalent struct (incl. OpenCV's operator& semantics), and the omp sections in
// the sort are dropped (they recurse on disjoint ranges, so the result is the same).
#include "vision/retinaface.h"

#include <algorithm>
#include <cmath>

namespace {

struct RectF {
  float x = 0, y = 0, width = 0, height = 0;
  float area() const { return width * height; }
};

struct FaceObject {
  RectF rect;
  float lmx[5], lmy[5];
  float prob;
};

// Same as cv::Rect_<float> a & b, then .area().
float intersection_area(const FaceObject& a, const FaceObject& b) {
  const RectF& ra = a.rect;
  const RectF& rb = b.rect;
  float x1 = std::max(ra.x, rb.x);
  float y1 = std::max(ra.y, rb.y);
  float w = std::min(ra.x + ra.width, rb.x + rb.width) - x1;
  float h = std::min(ra.y + ra.height, rb.y + rb.height) - y1;
  if (w <= 0 || h <= 0) return 0.f;
  return w * h;
}

void qsort_descent_inplace(std::vector<FaceObject>& faceobjects, int left, int right) {
  int i = left;
  int j = right;
  float p = faceobjects[(left + right) / 2].prob;
  while (i <= j) {
    while (faceobjects[i].prob > p) i++;
    while (faceobjects[j].prob < p) j--;
    if (i <= j) {
      std::swap(faceobjects[i], faceobjects[j]);
      i++;
      j--;
    }
  }
  if (left < j) qsort_descent_inplace(faceobjects, left, j);
  if (i < right) qsort_descent_inplace(faceobjects, i, right);
}

void qsort_descent_inplace(std::vector<FaceObject>& faceobjects) {
  if (faceobjects.empty()) return;
  qsort_descent_inplace(faceobjects, 0, static_cast<int>(faceobjects.size()) - 1);
}

void nms_sorted_bboxes(const std::vector<FaceObject>& faceobjects, std::vector<int>& picked, float nms_threshold) {
  picked.clear();
  const int n = static_cast<int>(faceobjects.size());
  std::vector<float> areas(n);
  for (int i = 0; i < n; i++) areas[i] = faceobjects[i].rect.area();
  for (int i = 0; i < n; i++) {
    const FaceObject& a = faceobjects[i];
    int keep = 1;
    for (int j = 0; j < static_cast<int>(picked.size()); j++) {
      const FaceObject& b = faceobjects[picked[j]];
      float inter_area = intersection_area(a, b);
      float union_area = areas[i] + areas[picked[j]] - inter_area;
      if (inter_area / union_area > nms_threshold) keep = 0;
    }
    if (keep) picked.push_back(i);
  }
}

ncnn::Mat generate_anchors(int base_size, const ncnn::Mat& ratios, const ncnn::Mat& scales) {
  int num_ratio = ratios.w;
  int num_scale = scales.w;
  ncnn::Mat anchors;
  anchors.create(4, num_ratio * num_scale);
  const float cx = base_size * 0.5f;
  const float cy = base_size * 0.5f;
  for (int i = 0; i < num_ratio; i++) {
    float ar = ratios[i];
    int r_w = round(base_size / sqrt(ar));
    int r_h = round(r_w * ar);
    for (int j = 0; j < num_scale; j++) {
      float scale = scales[j];
      float rs_w = r_w * scale;
      float rs_h = r_h * scale;
      float* anchor = anchors.row(i * num_scale + j);
      anchor[0] = cx - rs_w * 0.5f;
      anchor[1] = cy - rs_h * 0.5f;
      anchor[2] = cx + rs_w * 0.5f;
      anchor[3] = cy + rs_h * 0.5f;
    }
  }
  return anchors;
}

void generate_proposals(const ncnn::Mat& anchors, int feat_stride, const ncnn::Mat& score_blob,
                        const ncnn::Mat& bbox_blob, const ncnn::Mat& landmark_blob, float prob_threshold,
                        std::vector<FaceObject>& faceobjects) {
  int w = score_blob.w;
  int h = score_blob.h;
  const int num_anchors = anchors.h;
  for (int q = 0; q < num_anchors; q++) {
    const float* anchor = anchors.row(q);
    const ncnn::Mat score = score_blob.channel(q + num_anchors);
    const ncnn::Mat bbox = bbox_blob.channel_range(q * 4, 4);
    const ncnn::Mat landmark = landmark_blob.channel_range(q * 10, 10);
    float anchor_y = anchor[1];
    float anchor_w = anchor[2] - anchor[0];
    float anchor_h = anchor[3] - anchor[1];
    for (int i = 0; i < h; i++) {
      float anchor_x = anchor[0];
      for (int j = 0; j < w; j++) {
        int index = i * w + j;
        float prob = score[index];
        if (prob >= prob_threshold) {
          float dx = bbox.channel(0)[index];
          float dy = bbox.channel(1)[index];
          float dw = bbox.channel(2)[index];
          float dh = bbox.channel(3)[index];
          float cx = anchor_x + anchor_w * 0.5f;
          float cy = anchor_y + anchor_h * 0.5f;
          float pb_cx = cx + anchor_w * dx;
          float pb_cy = cy + anchor_h * dy;
          float pb_w = anchor_w * exp(dw);
          float pb_h = anchor_h * exp(dh);
          float x0 = pb_cx - pb_w * 0.5f;
          float y0 = pb_cy - pb_h * 0.5f;
          float x1 = pb_cx + pb_w * 0.5f;
          float y1 = pb_cy + pb_h * 0.5f;
          FaceObject obj;
          obj.rect.x = x0;
          obj.rect.y = y0;
          obj.rect.width = x1 - x0 + 1;
          obj.rect.height = y1 - y0 + 1;
          for (int k = 0; k < 5; k++) {
            obj.lmx[k] = cx + (anchor_w + 1) * landmark.channel(2 * k)[index];
            obj.lmy[k] = cy + (anchor_h + 1) * landmark.channel(2 * k + 1)[index];
          }
          obj.prob = prob;
          faceobjects.push_back(obj);
        }
        anchor_x += feat_stride;
      }
      anchor_y += feat_stride;
    }
  }
}

}  // namespace

bool RetinaFace::load(const std::string& model_base, int num_threads) {
  // Same options as the stock loadModel(): lightmode, blocktime 0 (no OpenMP spin-wait).
  net_.opt.lightmode = true;
  net_.opt.openmp_blocktime = 0;
  net_.opt.num_threads = num_threads;
  loaded_ = net_.load_param((model_base + ".param").c_str()) == 0 &&
            net_.load_model((model_base + ".bin").c_str()) == 0;
  return loaded_;
}

std::vector<FaceBox> RetinaFace::detect(const uint8_t* rgb, int img_w, int img_h, float prob_threshold,
                                        float nms_threshold) {
  std::vector<FaceBox> result;
  if (!loaded_) return result;
  ncnn::Mat in = ncnn::Mat::from_pixels(rgb, ncnn::Mat::PIXEL_RGB, img_w, img_h);
  ncnn::Extractor ex = net_.create_extractor();
  ex.input("data", in);

  std::vector<FaceObject> faceproposals;
  struct Level {
    int stride;
    float s0, s1;
  };
  for (const Level& lv : {Level{32, 32.f, 16.f}, Level{16, 8.f, 4.f}, Level{8, 2.f, 1.f}}) {
    ncnn::Mat score_blob, bbox_blob, landmark_blob;
    const std::string s = std::to_string(lv.stride);
    ex.extract(("face_rpn_cls_prob_reshape_stride" + s).c_str(), score_blob);
    ex.extract(("face_rpn_bbox_pred_stride" + s).c_str(), bbox_blob);
    ex.extract(("face_rpn_landmark_pred_stride" + s).c_str(), landmark_blob);
    const int base_size = 16;
    ncnn::Mat ratios(1);
    ratios[0] = 1.f;
    ncnn::Mat scales(2);
    scales[0] = lv.s0;
    scales[1] = lv.s1;
    ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);
    std::vector<FaceObject> objs;
    generate_proposals(anchors, lv.stride, score_blob, bbox_blob, landmark_blob, prob_threshold, objs);
    faceproposals.insert(faceproposals.end(), objs.begin(), objs.end());
  }

  qsort_descent_inplace(faceproposals);
  std::vector<int> picked;
  nms_sorted_bboxes(faceproposals, picked, nms_threshold);

  for (int idx : picked) {
    const FaceObject& o = faceproposals[idx];
    // clip to image size (stock behaviour)
    float x0 = o.rect.x;
    float y0 = o.rect.y;
    float x1 = x0 + o.rect.width;
    float y1 = y0 + o.rect.height;
    x0 = std::max(std::min(x0, (float)img_w - 1), 0.f);
    y0 = std::max(std::min(y0, (float)img_h - 1), 0.f);
    x1 = std::max(std::min(x1, (float)img_w - 1), 0.f);
    y1 = std::max(std::min(y1, (float)img_h - 1), 0.f);
    FaceBox b;
    b.x = x0;
    b.y = y0;
    b.w = x1 - x0;
    b.h = y1 - y0;
    b.prob = o.prob;
    for (int k = 0; k < 5; k++) {
      b.lm[k][0] = o.lmx[k];
      b.lm[k][1] = o.lmy[k];
    }
    result.push_back(b);
  }
  return result;
}

void scale_faces(std::vector<FaceBox>& faces, float k) {
  for (FaceBox& b : faces) {
    b.x *= k;
    b.y *= k;
    b.w *= k;
    b.h *= k;
    for (auto& p : b.lm) {
      p[0] *= k;
      p[1] *= k;
    }
  }
}

void resize_rgb(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh) {
  ncnn::resize_bilinear_c3(src, sw, sh, dst, dw, dh);
}
