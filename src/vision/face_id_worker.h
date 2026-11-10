#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "vision/face_db.h"
#include "vision/retinaface.h"

// One on-demand face-ID request (design §4), queued by the IO thread for the detect thread.
struct FaceJob {
  enum Kind { kLearn, kWho, kForget, kList } kind = kWho;
  int chan = 0;
  long long req_id = 0;
  std::string name;  // learn / forget
  int face_id = 0;   // tracker id; 0 = largest face
  int64_t enq_ms = 0;
};
struct JobReply {
  int chan;
  std::string line;  // complete reply line, without '\n'
};

// IO thread -> detect thread. At most kMax jobs in flight (queued or being worked on);
// the detect thread calls finished(n) for every n replies it sends.
class FaceJobQueue {
 public:
  static constexpr size_t kMax = 4;
  bool push(FaceJob j);
  std::vector<FaceJob> take_all();
  void finished(size_t n);

 private:
  std::mutex mu_;
  std::vector<FaceJob> q_;
  size_t inflight_ = 0;
};

// Detect-thread side of face ID: who-first scheduling (one embed per frame), learn sampling
// (5 samples >= 150 ms apart on one track id), 3 s deadlines from enqueue, and the det-line
// name write-back. Owns no model: embed() is passed in.
class FaceIdWorker {
 public:
  using EmbedFn = std::function<Embedding(const FaceBox&)>;
  static constexpr int kLearnSamples = 5;
  static constexpr int64_t kSampleGapMs = 150, kDeadlineMs = 3000, kNameForgetMs = 1000;
  FaceIdWorker(FaceDb& db, std::function<bool(const FaceDb&)> persist) : db_(db), persist_(std::move(persist)) {}
  void enqueue(std::vector<FaceJob> jobs);
  // Call once per frame on which face detection ran (faces/ids from FaceTracker); also answers
  // deadlines. forget / faces jobs need no faces: call with empty vectors in standby.
  std::vector<JobReply> step(const std::vector<FaceBox>& faces, const std::vector<int>& ids, uint64_t frame,
                             int64_t now_ms, float th, const EmbedFn& embed);
  // Deadlines only, for frames without face detection. ids_in_view: last known face ids.
  std::vector<JobReply> expire(int64_t now_ms, const std::vector<int>& ids_in_view);
  std::string name_for(int track_id) const;  // "" = none

 private:
  struct Pending {
    FaceJob job;
    int track = 0;  // learn: track being sampled (0 = not started)
    std::vector<Embedding> samples;
    int64_t last_sample_ms = 0;
  };
  struct Named {
    std::string name;
    int64_t seen_ms;
  };
  FaceDb& db_;
  std::function<bool(const FaceDb&)> persist_;
  std::deque<Pending> pending_;
  std::map<int, Named> names_;
};
