#include "vision/face_id_worker.h"

#include <algorithm>
#include <cstdio>

#include "proto/flat_json.h"

// ---- FaceJobQueue ----

bool FaceJobQueue::push(FaceJob j) {
  std::lock_guard<std::mutex> lk(mu_);
  if (inflight_ >= kMax) return false;
  inflight_++;
  q_.push_back(std::move(j));
  return true;
}

std::vector<FaceJob> FaceJobQueue::take_all() {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<FaceJob> out;
  out.swap(q_);
  return out;
}

void FaceJobQueue::finished(size_t n) {
  std::lock_guard<std::mutex> lk(mu_);
  inflight_ = n > inflight_ ? 0 : inflight_ - n;
}

// ---- FaceIdWorker ----

namespace {
std::string head(long long id) { return "{\"id\":" + std::to_string(id) + ",\"ok\":true"; }
std::string fail(long long id, const char* code) {
  return "{\"id\":" + std::to_string(id) + ",\"ok\":false,\"err\":\"" + code + "\"}";
}
bool overlap(const FaceBox& a, const FaceBox& b) {
  return std::min(a.x + a.w, b.x + b.w) > std::max(a.x, b.x) && std::min(a.y + a.h, b.y + b.h) > std::max(a.y, b.y);
}
// Index of the job's target face: the given tracker id, or the largest face; -1 = none.
int target(const FaceJob& j, int track, const std::vector<FaceBox>& faces, const std::vector<int>& ids) {
  const int want = track ? track : j.face_id;
  if (want) {
    for (size_t i = 0; i < ids.size(); i++)
      if (ids[i] == want) return static_cast<int>(i);
    return -1;
  }
  int best = -1;
  for (size_t i = 0; i < faces.size(); i++)
    if (best < 0 || faces[i].w * faces[i].h > faces[best].w * faces[best].h) best = static_cast<int>(i);
  return best;
}
}  // namespace

void FaceIdWorker::enqueue(std::vector<FaceJob> jobs) {
  for (FaceJob& j : jobs) pending_.push_back(Pending{std::move(j), 0, {}, 0});
}

std::string FaceIdWorker::name_for(int track_id) const {
  auto it = names_.find(track_id);
  return it == names_.end() ? std::string() : it->second.name;
}

std::vector<JobReply> FaceIdWorker::expire(int64_t now_ms, const std::vector<int>& ids_in_view) {
  std::vector<JobReply> out;
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (now_ms - it->job.enq_ms < kDeadlineMs) {
      ++it;
      continue;
    }
    const char* code = "timeout";
    if (it->job.kind == FaceJob::kLearn) {
      const bool present = it->track ? std::find(ids_in_view.begin(), ids_in_view.end(), it->track) != ids_in_view.end()
                                     : !ids_in_view.empty();
      if (!present) code = "noface";
    }
    out.push_back(JobReply{it->job.chan, fail(it->job.req_id, code)});
    it = pending_.erase(it);
  }
  return out;
}

std::vector<JobReply> FaceIdWorker::step(const std::vector<FaceBox>& faces, const std::vector<int>& ids, uint64_t frame,
                                         int64_t now_ms, float th, const EmbedFn& embed) {
  // Name write-back bookkeeping: refresh seen tracks, drop overlapping faces (ids may swap),
  // forget names whose track has been out of view for more than 1 s.
  for (int id : ids) {
    auto it = names_.find(id);
    if (it != names_.end()) it->second.seen_ms = now_ms;
  }
  for (size_t a = 0; a < faces.size(); a++)
    for (size_t b = a + 1; b < faces.size(); b++)
      if (overlap(faces[a], faces[b])) {
        names_.erase(ids[a]);
        names_.erase(ids[b]);
      }
  for (auto it = names_.begin(); it != names_.end();)
    it = now_ms - it->second.seen_ms > kNameForgetMs ? names_.erase(it) : std::next(it);

  std::vector<JobReply> out = expire(now_ms, ids);

  // forget / faces: no inference; who without a usable face: noface at once.
  for (auto it = pending_.begin(); it != pending_.end();) {
    const FaceJob& j = it->job;
    if (j.kind == FaceJob::kList) {
      std::string s = head(j.req_id) + ",\"faces\":[";
      bool first = true;
      for (const auto& [name, n] : db_.list()) {
        s += std::string(first ? "" : ",") + "{\"name\":" + json_str(name) + ",\"samples\":" + std::to_string(n) + "}";
        first = false;
      }
      out.push_back(JobReply{j.chan, s + "]}"});
    } else if (j.kind == FaceJob::kForget) {
      const FaceDb backup = db_;
      if (!db_.forget(j.name)) {
        out.push_back(JobReply{j.chan, fail(j.req_id, "badarg")});
      } else if (!persist_(db_)) {
        db_ = backup;
        out.push_back(JobReply{j.chan, fail(j.req_id, "io")});
      } else {
        for (auto n = names_.begin(); n != names_.end();) n = n->second.name == j.name ? names_.erase(n) : std::next(n);
        out.push_back(JobReply{j.chan, head(j.req_id) + ",\"name\":" + json_str(j.name) + "}"});
      }
    } else if (j.kind == FaceJob::kWho && target(j, 0, faces, ids) < 0) {
      out.push_back(JobReply{j.chan, fail(j.req_id, "noface")});
    } else {
      ++it;
      continue;
    }
    it = pending_.erase(it);
  }

  // One embed per frame: the oldest who first, else the oldest learn.
  auto who = std::find_if(pending_.begin(), pending_.end(), [](const Pending& p) { return p.job.kind == FaceJob::kWho; });
  if (who != pending_.end()) {
    const int k = target(who->job, 0, faces, ids);
    const Match m = db_.best(embed(faces[k]), th);
    char buf[64];
    std::snprintf(buf, sizeof buf, ",\"sim\":%.3f}", m.sim);
    out.push_back(JobReply{who->job.chan, head(who->job.req_id) + ",\"f\":" + std::to_string(frame) + ",\"face\":" +
                                              std::to_string(ids[k]) + ",\"name\":" +
                                              (m.name.empty() ? std::string("null") : json_str(m.name)) + buf});
    if (m.name.empty()) names_.erase(ids[k]);
    else names_[ids[k]] = Named{m.name, now_ms};
    pending_.erase(who);
    return out;
  }
  auto learn = std::find_if(pending_.begin(), pending_.end(), [](const Pending& p) { return p.job.kind == FaceJob::kLearn; });
  if (learn == pending_.end()) return out;
  const int k = target(learn->job, learn->track, faces, ids);
  if (k < 0) return out;  // target not in view this frame: wait (deadline decides)
  if (!learn->samples.empty() && now_ms - learn->last_sample_ms < kSampleGapMs) return out;
  learn->track = ids[k];
  learn->samples.push_back(embed(faces[k]));
  learn->last_sample_ms = now_ms;
  if (static_cast<int>(learn->samples.size()) < kLearnSamples) return out;

  const FaceJob& j = learn->job;
  const FaceDb backup = db_;
  std::string other;
  std::string reply;
  switch (db_.add(j.name, learn->samples, th, &other)) {
    case AddResult::kOk:
      if (!persist_(db_)) {
        db_ = backup;
        reply = fail(j.req_id, "io");
        break;
      }
      for (const auto& [name, n] : db_.list())
        if (name == j.name)
          reply = head(j.req_id) + ",\"name\":" + json_str(j.name) + ",\"samples\":" + std::to_string(n) +
                  ",\"f\":" + std::to_string(frame) + "}";
      names_[learn->track] = Named{j.name, now_ms};
      break;
    case AddResult::kFull:
      reply = fail(j.req_id, "full");
      break;
    case AddResult::kConflict:
      reply = "{\"id\":" + std::to_string(j.req_id) + ",\"ok\":false,\"err\":\"conflict\",\"name\":" + json_str(other) + "}";
      break;
    case AddResult::kBadName:
      reply = fail(j.req_id, "badarg");
      break;
  }
  out.push_back(JobReply{j.chan, reply});
  pending_.erase(learn);
  return out;
}
