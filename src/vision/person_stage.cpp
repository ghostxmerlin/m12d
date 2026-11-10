#include "vision/person_stage.h"

#include "vision/pairing.h"

std::vector<PersonOut> PersonStage::step(const uint8_t* rgb320, bool ran_person, const std::vector<FaceBox>& faces,
                                         const std::vector<int>& face_ids, int64_t now_ms) {
  if (ran_person) {
    tracker_.update(det_.detect(rgb320, 320, 240, kSize, kProb), now_ms);
    if (tracker_.needs_verify(now_ms)) tracker_.apply_verify(det_.detect(rgb320, 320, 240, kVerifySize, kVerifyProb), now_ms);
  }
  const std::vector<PersonTrack> tracks = tracker_.confirmed();
  std::vector<PersonBox> boxes;
  boxes.reserve(tracks.size());
  for (const PersonTrack& t : tracks) boxes.push_back(t.box);
  const std::vector<int> pair = pair_faces(faces, boxes);
  std::vector<PersonOut> out;
  out.reserve(tracks.size());
  for (size_t i = 0; i < tracks.size(); i++) {
    const int k = pair[i];
    out.push_back(PersonOut{tracks[i].id, tracks[i].box, k >= 0 && k < static_cast<int>(face_ids.size()) ? face_ids[k] : -1});
  }
  return out;
}
