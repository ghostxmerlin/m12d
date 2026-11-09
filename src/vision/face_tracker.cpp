#include "vision/face_tracker.h"

#include <algorithm>
#include <tuple>

namespace {
float iou(const FaceBox& a, const FaceBox& b) {
  const float w = std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x);
  const float h = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y);
  if (w <= 0 || h <= 0) return 0.f;
  const float inter = w * h;
  return inter / (a.w * a.h + b.w * b.h - inter);
}
}  // namespace

std::vector<int> FaceTracker::update(const std::vector<FaceBox>& faces, int64_t now_ms) {
  tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                               [&](const Track& t) { return now_ms - t.last_seen_ms > kForgetMs; }),
                tracks_.end());
  std::vector<std::tuple<float, size_t, size_t>> pairs;  // (iou, face, track)
  for (size_t f = 0; f < faces.size(); f++)
    for (size_t t = 0; t < tracks_.size(); t++) {
      const float v = iou(faces[f], tracks_[t].box);
      if (v >= kMatchIou) pairs.emplace_back(v, f, t);
    }
  std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
  std::vector<int> ids(faces.size(), 0);
  std::vector<bool> used(tracks_.size(), false);
  for (const auto& [v, f, t] : pairs) {
    if (ids[f] || used[t]) continue;
    used[t] = true;
    ids[f] = tracks_[t].id;
    tracks_[t].box = faces[f];
    tracks_[t].last_seen_ms = now_ms;
  }
  for (size_t f = 0; f < faces.size(); f++)
    if (!ids[f]) {
      Track t{next_id_++, faces[f], now_ms};
      ids[f] = t.id;
      tracks_.push_back(t);
    }
  return ids;
}

const FaceTracker::Track* FaceTracker::find(int id) const {
  for (const Track& t : tracks_)
    if (t.id == id) return &t;
  return nullptr;
}

int FaceTracker::pick_for_attr(const std::vector<int>& ids, int64_t now_ms) const {
  int best = -1;
  std::pair<int, int64_t> best_key;  // (samples, last_sample): fewer, then older, first
  for (size_t i = 0; i < ids.size(); i++) {
    const Track* t = find(ids[i]);
    if (!t) continue;
    const int64_t since = now_ms - t->last_sample_ms;
    const bool due = t->samples < kFirstSamples ? since >= kSampleGapMs : since >= kRefreshMs;
    if (!due) continue;
    const std::pair<int, int64_t> key{std::min(t->samples, kFirstSamples), t->last_sample_ms};
    if (best < 0 || key < best_key) {
      best = static_cast<int>(i);
      best_key = key;
    }
  }
  return best;
}

void FaceTracker::add_sample(int id, const GenderAgeResult& r, int64_t now_ms) {
  for (Track& t : tracks_)
    if (t.id == id) {
      t.samples++;
      t.last_sample_ms = now_ms;
      t.recent.push_back(r);
      if (t.recent.size() > static_cast<size_t>(kKeepSamples)) t.recent.pop_front();
    }
}

FaceAttr FaceTracker::attr(int id) const {
  FaceAttr a;
  const Track* t = find(id);
  if (!t || t->recent.empty()) return a;
  a.have = true;
  int male = 0, female = 0;
  std::vector<int> ages;
  for (const GenderAgeResult& r : t->recent) {
    ages.push_back(r.age);
    if (r.margin >= kMinMargin) (r.gender == 1 ? male : female)++;
  }
  a.gender = male > female ? 1 : female > male ? 0 : -1;
  std::sort(ages.begin(), ages.end());
  a.age = ages[(ages.size() - 1) / 2];
  return a;
}
