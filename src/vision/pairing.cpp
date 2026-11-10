#include "vision/pairing.h"

#include <algorithm>
#include <tuple>

std::vector<int> pair_faces(const std::vector<FaceBox>& faces, const std::vector<PersonBox>& persons) {
  // Candidate pairs, smallest person box first; greedy one-to-one assignment.
  std::vector<std::tuple<float, size_t, size_t>> cand;  // (person area, person, face)
  for (size_t p = 0; p < persons.size(); p++) {
    const PersonBox& b = persons[p];
    for (size_t f = 0; f < faces.size(); f++) {
      const float cx = faces[f].x + faces[f].w / 2, cy = faces[f].y + faces[f].h / 2;
      if (cx >= b.x && cx <= b.x + b.w && cy >= b.y && cy <= b.y + 0.4f * b.h) cand.emplace_back(b.w * b.h, p, f);
    }
  }
  std::stable_sort(cand.begin(), cand.end(), [](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });
  std::vector<int> out(persons.size(), -1);
  std::vector<bool> face_used(faces.size(), false);
  for (const auto& [area, p, f] : cand) {
    if (out[p] >= 0 || face_used[f]) continue;
    out[p] = static_cast<int>(f);
    face_used[f] = true;
  }
  return out;
}
