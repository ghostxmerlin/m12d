#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "vision/face_tracker.h"
#include "vision/person_det.h"
#include "vision/retinaface.h"

// One detection line, field-compatible with the stock face_detector output:
// {"f":<frame>,"num":N,"face":[{"x","y","w","h","prob","mark":[{"x","y"}x5]}...],"running":"Face Detector"}
// Coordinates are multiplied by scale (detector space -> 640x480). Box values have 1 decimal,
// prob 3 decimals; marks are truncated to int before scaling, like stock. Keeps the max_faces
// highest-prob faces, highest first. Emitted even with zero faces (stock emits nothing then).
// Per-face extras appended after the stock fields: "id" (tracker), and once known
// "gender" ("m" / "f" / "?"), "age", "age_group".
struct FaceExtra {
  int id = 0;
  FaceAttr attr;
};
// Persons (phase 1b): when persons != nullptr, ',"person":[...]' is appended after "running":
// {"x","y","w","h","prob","id","face"} (face = paired face id or -1), at most kMaxPersons, by prob.
struct PersonOut {
  int id;
  PersonBox box;
  int face_id;
};
constexpr size_t kMaxPersons = 5;
std::string det_json(uint64_t frame_id, const std::vector<FaceBox>& faces, float scale, size_t max_faces,
                     bool with_lm = true, const std::vector<FaceExtra>* extra = nullptr,
                     const std::vector<PersonOut>* persons = nullptr);

// Heartbeat object body: {"t":..,"fps":..,"tier":..,"up":..,"mem":..,"frames":..,"reopens":..,"drop":..,"mhz":..}
std::string hb_body(int temp_c, float fps, int tier, int64_t uptime_s, long mem_avail_kb, uint64_t frames,
                    uint64_t reopens, uint64_t dropped_lines, float cpu_mhz);
// Heartbeat line: {"hb":<hb_body>}
std::string hb_json(int temp_c, float fps, int tier, int64_t uptime_s, long mem_avail_kb, uint64_t frames,
                    uint64_t reopens, uint64_t dropped_lines, float cpu_mhz);

// JSON members (no surrounding braces) for the snap reply:
// "f":<frame>,"path":"<path>","num":N,"face":[...]  (same face objects as det_json)
std::string snap_members(uint64_t frame_id, const std::string& path, const std::vector<FaceBox>& faces, float scale,
                         size_t max_faces, bool with_lm = true);
