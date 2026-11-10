#include <cmath>
#include <string>
#include <vector>
#include "doctest.h"
#include "vision/face_id_worker.h"

namespace {
Embedding axis(int k) {
  Embedding e{};
  e[k % 128] = 1.f;
  return e;
}
FaceBox fb(float x, float y = 50, float w = 40, float h = 50) { return FaceBox{x, y, w, h, 0.95f, {}}; }
// Fake embedder: a face left of x=100 looks like axis 0, otherwise like axis 50. Counts calls.
struct FakeEmbed {
  int calls = 0;
  FaceIdWorker::EmbedFn fn() {
    return [this](const FaceBox& f) {
      calls++;
      return f.x < 100 ? axis(0) : axis(50);
    };
  }
};
FaceJob job(FaceJob::Kind k, long long id, std::string name = "", int face = 0, int64_t t = 0) {
  FaceJob j;
  j.kind = k;
  j.chan = 0;
  j.req_id = id;
  j.name = std::move(name);
  j.face_id = face;
  j.enq_ms = t;
  return j;
}
struct W {
  FaceDb db;
  int persists = 0;
  bool persist_ok = true;
  FaceIdWorker w{db, [this](const FaceDb&) {
                   persists++;
                   return persist_ok;
                 }};
  FakeEmbed e;
  std::vector<JobReply> step(const std::vector<FaceBox>& f, const std::vector<int>& ids, int64_t t, uint64_t frame = 7) {
    return w.step(f, ids, frame, t, 0.5f, e.fn());
  }
};
}  // namespace

TEST_CASE("face job queue caps in-flight jobs at 4") {
  FaceJobQueue q;
  for (int i = 0; i < 4; i++) CHECK(q.push(job(FaceJob::kWho, i)));
  CHECK_FALSE(q.push(job(FaceJob::kWho, 9)));
  CHECK(q.take_all().size() == 4);
  CHECK_FALSE(q.push(job(FaceJob::kWho, 9)));  // taken but not finished: still in flight
  q.finished(1);
  CHECK(q.push(job(FaceJob::kWho, 9)));
}

TEST_CASE("who on empty db returns null name sim 0") {
  W t;
  t.w.enqueue({job(FaceJob::kWho, 1)});
  auto r = t.step({fb(10)}, {11}, 0);
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":1,"ok":true,"f":7,"face":11,"name":null,"sim":0.000})");
}

TEST_CASE("who finds learned name and writes it back") {
  W t;
  REQUIRE(t.db.add("alice", {axis(0)}, 0.5f, nullptr) == AddResult::kOk);
  t.w.enqueue({job(FaceJob::kWho, 2)});
  auto r = t.step({fb(10)}, {11}, 0);
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":2,"ok":true,"f":7,"face":11,"name":"alice","sim":1.000})");
  CHECK(t.w.name_for(11) == "alice");
  CHECK(t.w.name_for(12).empty());
}

TEST_CASE("who picks the largest face, or the given id, and noface for a missing id") {
  W t;
  REQUIRE(t.db.add("alice", {axis(0)}, 0.5f, nullptr) == AddResult::kOk);
  t.w.enqueue({job(FaceJob::kWho, 1), job(FaceJob::kWho, 2, "", 11), job(FaceJob::kWho, 3, "", 99)});
  const std::vector<FaceBox> f = {fb(10, 50, 30, 40), fb(300, 50, 60, 70)};
  auto r1 = t.step(f, {11, 12}, 0);
  REQUIRE(r1.size() == 2);  // id 3 answered at once with noface; id 1 embedded
  CHECK(r1[0].line == R"({"id":3,"ok":false,"err":"noface"})");
  CHECK(r1[1].line.find("\"face\":12,\"name\":null") != std::string::npos);  // largest = right face
  auto r2 = t.step(f, {11, 12}, 40);
  REQUIRE(r2.size() == 1);
  CHECK(r2[0].line.find("\"face\":11,\"name\":\"alice\"") != std::string::npos);
}

TEST_CASE("who with no face in view replies noface at once") {
  W t;
  t.w.enqueue({job(FaceJob::kWho, 4)});
  auto r = t.step({}, {}, 0);
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":4,"ok":false,"err":"noface"})");
}

TEST_CASE("at most one embed per step") {
  W t;
  t.w.enqueue({job(FaceJob::kWho, 1), job(FaceJob::kWho, 2)});
  CHECK(t.step({fb(10)}, {11}, 0).size() == 1);
  CHECK(t.e.calls == 1);
  CHECK(t.step({fb(10)}, {11}, 40).size() == 1);
  CHECK(t.e.calls == 2);
}

TEST_CASE("learn takes 5 samples >= 150 ms apart from the same id then stores") {
  W t;
  t.w.enqueue({job(FaceJob::kLearn, 5, "alice")});
  std::vector<JobReply> r;
  int64_t ms = 0;
  for (; ms <= 1000 && r.empty(); ms += 50) r = t.step({fb(10)}, {11}, ms, 100 + ms);
  REQUIRE(r.size() == 1);
  CHECK(t.e.calls == 5);
  CHECK(ms - 50 == 600);  // samples at 0,150,300,450,600
  CHECK(r[0].line == R"({"id":5,"ok":true,"name":"alice","samples":5,"f":700})");
  CHECK(t.persists == 1);
  CHECK(t.db.list().size() == 1);
  CHECK(t.w.name_for(11) == "alice");
}

TEST_CASE("who has priority over an in-progress learn") {
  W t;
  t.w.enqueue({job(FaceJob::kLearn, 5, "alice")});
  t.step({fb(10)}, {11}, 0);  // learn sample 1
  t.w.enqueue({job(FaceJob::kWho, 6)});
  auto r = t.step({fb(10)}, {11}, 200);  // learn is due too, but who goes first
  REQUIRE(r.size() == 1);
  CHECK(r[0].line.rfind(R"({"id":6,)", 0) == 0);
  CHECK(t.e.calls == 2);
}

TEST_CASE("learn aborts with noface when the sampled id disappears and nothing is stored") {
  W t;
  t.w.enqueue({job(FaceJob::kLearn, 5, "alice")});
  t.step({fb(10)}, {11}, 0);
  t.step({fb(10)}, {11}, 150);
  std::vector<JobReply> r;
  for (int64_t ms = 300; ms <= 3000 && r.empty(); ms += 50) r = t.step({fb(300)}, {12}, ms);  // someone else
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":5,"ok":false,"err":"noface"})");
  CHECK(t.e.calls == 2);
  CHECK(t.db.list().empty());
  CHECK(t.persists == 0);
}

TEST_CASE("learn conflict and full replies, nothing persisted") {
  W t;
  REQUIRE(t.db.add("bob", {axis(0)}, 0.5f, nullptr) == AddResult::kOk);
  t.w.enqueue({job(FaceJob::kLearn, 5, "alice")});
  std::vector<JobReply> r;
  for (int64_t ms = 0; ms <= 1000 && r.empty(); ms += 150) r = t.step({fb(10)}, {11}, ms);
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":5,"ok":false,"err":"conflict","name":"bob"})");
  CHECK(t.persists == 0);
  for (int i = 0; i < 15; i++) REQUIRE(t.db.add("p" + std::to_string(i), {axis(60 + i)}, 0.5f, nullptr) == AddResult::kOk);
  t.w.enqueue({job(FaceJob::kLearn, 6, "carol")});
  r.clear();
  for (int64_t ms = 2000; ms <= 3000 && r.empty(); ms += 150) r = t.step({fb(300)}, {12}, ms);
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":6,"ok":false,"err":"full"})");
  CHECK(t.persists == 0);
}

TEST_CASE("persist failure -> io error and db unchanged") {
  W t;
  t.persist_ok = false;
  t.w.enqueue({job(FaceJob::kLearn, 5, "alice")});
  std::vector<JobReply> r;
  for (int64_t ms = 0; ms <= 1000 && r.empty(); ms += 150) r = t.step({fb(10)}, {11}, ms);
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":5,"ok":false,"err":"io"})");
  CHECK(t.db.list().empty());
  CHECK(t.w.name_for(11).empty());
}

TEST_CASE("deadline: who after 3 s -> timeout; learn with face present -> timeout, absent -> noface") {
  W t;
  t.w.enqueue({job(FaceJob::kWho, 1, "", 0, 0)});
  CHECK(t.w.expire(2999, {11}).empty());
  auto r = t.w.expire(3000, {11});
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":1,"ok":false,"err":"timeout"})");
  t.w.enqueue({job(FaceJob::kLearn, 2, "alice", 0, 4000), job(FaceJob::kLearn, 3, "bob", 0, 4000)});
  t.step({fb(10)}, {11}, 4000);  // learn 2 starts on id 11
  r = t.w.expire(7000, {11});
  REQUIRE(r.size() == 2);
  CHECK(r[0].line == R"({"id":2,"ok":false,"err":"timeout"})");  // its face is still there
  CHECK(r[1].line == R"({"id":3,"ok":false,"err":"timeout"})");  // not started, but a face is in view
  t.w.enqueue({job(FaceJob::kLearn, 4, "alice", 0, 8000)});
  t.step({fb(10)}, {11}, 8000);
  r = t.w.expire(11000, {});
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":4,"ok":false,"err":"noface"})");
  CHECK(t.db.list().empty());
}

TEST_CASE("forget and faces are answered without an embed") {
  W t;
  REQUIRE(t.db.add("alice", {axis(0)}, 0.5f, nullptr) == AddResult::kOk);
  t.w.enqueue({job(FaceJob::kList, 1), job(FaceJob::kForget, 2, "nobody"), job(FaceJob::kForget, 3, "alice")});
  auto r = t.step({}, {}, 0);
  REQUIRE(r.size() == 3);
  CHECK(r[0].line == R"({"id":1,"ok":true,"faces":[{"name":"alice","samples":1}]})");
  CHECK(r[1].line == R"({"id":2,"ok":false,"err":"badarg"})");
  CHECK(r[2].line == R"({"id":3,"ok":true,"name":"alice"})");
  CHECK(t.e.calls == 0);
  CHECK(t.persists == 1);
  CHECK(t.db.list().empty());
}

TEST_CASE("forget clears write-back on all tracks with that name") {
  W t;
  REQUIRE(t.db.add("alice", {axis(0)}, 0.5f, nullptr) == AddResult::kOk);
  t.w.enqueue({job(FaceJob::kWho, 1, "", 11), job(FaceJob::kWho, 2, "", 12)});
  const std::vector<FaceBox> f = {fb(10), fb(50, 200)};  // both look like alice, no overlap
  t.step(f, {11, 12}, 0);
  t.step(f, {11, 12}, 40);
  REQUIRE(t.w.name_for(11) == "alice");
  REQUIRE(t.w.name_for(12) == "alice");
  t.w.enqueue({job(FaceJob::kForget, 3, "alice")});
  t.step(f, {11, 12}, 80);
  CHECK(t.w.name_for(11).empty());
  CHECK(t.w.name_for(12).empty());
}

TEST_CASE("write-back: overlapping faces drop it, null clears, another name replaces, unseen 1 s forgets") {
  W t;
  REQUIRE(t.db.add("alice", {axis(0)}, 0.5f, nullptr) == AddResult::kOk);
  REQUIRE(t.db.add("bob", {axis(50)}, 0.5f, nullptr) == AddResult::kOk);
  t.w.enqueue({job(FaceJob::kWho, 1)});
  t.step({fb(10)}, {11}, 0);
  REQUIRE(t.w.name_for(11) == "alice");
  t.step({fb(10), fb(30)}, {11, 12}, 40);  // boxes overlap: ids may swap
  CHECK(t.w.name_for(11).empty());
  t.w.enqueue({job(FaceJob::kWho, 2)});
  t.step({fb(10)}, {11}, 80);
  REQUIRE(t.w.name_for(11) == "alice");
  t.w.enqueue({job(FaceJob::kWho, 3)});
  t.step({fb(300)}, {11}, 120);  // same id now looks like bob
  CHECK(t.w.name_for(11) == "bob");
  t.db.forget("bob");
  t.w.enqueue({job(FaceJob::kWho, 4)});
  t.step({fb(300)}, {11}, 160);  // nobody matches now -> null clears
  CHECK(t.w.name_for(11).empty());
  t.w.enqueue({job(FaceJob::kWho, 5)});
  t.step({fb(10)}, {11}, 200);
  REQUIRE(t.w.name_for(11) == "alice");
  t.step({}, {}, 1200);
  CHECK(t.w.name_for(11) == "alice");  // exactly 1 s unseen: kept
  t.step({}, {}, 1201);
  CHECK(t.w.name_for(11).empty());
}

TEST_CASE("learn refuses to mix two people on one track id") {
  // The IoU tracker can hand an id to someone else who steps into the same spot within its
  // forget window; samples that do not match the first one must not be stored together.
  W t;
  t.w.enqueue({job(FaceJob::kLearn, 5, "alice")});
  t.step({fb(10)}, {11}, 0);  // sample 1 looks like axis 0
  t.step({fb(10)}, {11}, 150);
  std::vector<JobReply> r;
  for (int64_t ms = 300; ms <= 3000 && r.empty(); ms += 150) r = t.step({fb(300)}, {11}, ms);  // same id, axis 50
  REQUIRE(r.size() == 1);
  CHECK(r[0].line == R"({"id":5,"ok":false,"err":"noface"})");
  CHECK(t.db.list().empty());
  CHECK(t.persists == 0);
}
