#include "vision/person_tracker.h"

#include <algorithm>
#include <tuple>

namespace {
float iou(const PersonBox& a, const PersonBox& b) {
  const float w = std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x);
  const float h = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y);
  if (w <= 0 || h <= 0) return 0.f;
  const float inter = w * h;
  return inter / (a.w * a.h + b.w * b.h - inter);
}
bool center_inside(const PersonBox& a, const PersonBox& b) {
  const float cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  return cx >= b.x && cx <= b.x + b.w && cy >= b.y && cy <= b.y + b.h;
}
}  // namespace

void PersonTracker::update(const std::vector<PersonBox>& dets, int64_t now_ms) {
  tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                               [&](const Track& t) { return now_ms - t.last_seen_ms > kForgetMs; }),
                tracks_.end());
  std::vector<std::tuple<float, size_t, size_t>> pairs;  // (iou, det, track)
  for (size_t d = 0; d < dets.size(); d++)
    for (size_t t = 0; t < tracks_.size(); t++) {
      const float v = iou(dets[d], tracks_[t].t.box);
      if (v >= kMatchIou) pairs.emplace_back(v, d, t);
    }
  std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
  std::vector<bool> det_used(dets.size(), false), trk_used(tracks_.size(), false);
  for (const auto& [v, d, t] : pairs) {
    if (det_used[d] || trk_used[t]) continue;
    det_used[d] = trk_used[t] = true;
    tracks_[t].t.box = dets[d];
    tracks_[t].last_seen_ms = now_ms;
  }
  for (size_t d = 0; d < dets.size(); d++)
    if (!det_used[d]) tracks_.push_back(Track{PersonTrack{next_id_++, dets[d], PersonTrack::kPending}, now_ms, 0});
}

bool PersonTracker::needs_verify(int64_t now_ms) const {
  for (const Track& t : tracks_)
    if (t.t.state == PersonTrack::kPending ||
        (t.t.state == PersonTrack::kRejected && now_ms - t.verified_ms >= kRecheckMs))
      return true;
  return false;
}

void PersonTracker::apply_verify(const std::vector<PersonBox>& dets, int64_t now_ms) {
  for (Track& t : tracks_) {
    const bool due = t.t.state == PersonTrack::kPending ||
                     (t.t.state == PersonTrack::kRejected && now_ms - t.verified_ms >= kRecheckMs);
    if (!due) continue;
    bool ok = false;
    for (const PersonBox& d : dets)
      if (iou(t.t.box, d) >= kMatchIou || center_inside(t.t.box, d)) ok = true;
    t.t.state = ok ? PersonTrack::kConfirmed : PersonTrack::kRejected;
    t.verified_ms = now_ms;
  }
}

std::vector<PersonTrack> PersonTracker::confirmed() const {
  std::vector<PersonTrack> out;
  for (const Track& t : tracks_)
    if (t.t.state == PersonTrack::kConfirmed) out.push_back(t.t);
  return out;
}

void PersonTracker::clear() { tracks_.clear(); }
