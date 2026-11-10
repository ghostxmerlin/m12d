#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include "doctest.h"
#include "vision/face_db.h"

namespace {
// Unit vector along axis k, slightly tilted towards axis k+1 by t (cos = 1/sqrt(1+t^2)).
Embedding axis(int k, float t = 0.f) {
  Embedding e{};
  e[k % 128] = 1.f;
  e[(k + 1) % 128] = t;
  const float n = std::sqrt(1.f + t * t);
  for (float& v : e) v /= n;
  return e;
}
std::vector<Embedding> five(int k) { return {axis(k), axis(k, 0.1f), axis(k, 0.2f), axis(k), axis(k, 0.05f)}; }
std::string tmpdir() {
  char t[] = "/tmp/m12fdb.XXXXXX";
  REQUIRE(mkdtemp(t));
  return t;
}
std::vector<std::string> ls(const std::string& d) {
  std::vector<std::string> out;
  DIR* dp = opendir(d.c_str());
  REQUIRE(dp);
  while (dirent* e = readdir(dp))
    if (e->d_name[0] != '.') out.push_back(e->d_name);
  closedir(dp);
  return out;
}
std::string slurp(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), {});
}
void spit(const std::string& p, const std::string& s) { std::ofstream(p, std::ios::binary) << s; }
}  // namespace

TEST_CASE("add then best finds the name above threshold") {
  FaceDb db;
  REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  REQUIRE(db.add("bob", five(10), 0.5f, nullptr) == AddResult::kOk);
  Match m = db.best(axis(10, 0.3f), 0.5f);
  CHECK(m.name == "bob");
  CHECK(m.sim > 0.9f);
}

TEST_CASE("best returns empty below threshold but still gives sim") {
  FaceDb db;
  REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  // closest sample is axis(0, 0.2): cos = (1 + 0.6) / (sqrt(10) * sqrt(1.04)) ~ 0.496
  Match m = db.best(axis(0, 3.f), 0.5f);
  CHECK(m.name.empty());
  CHECK(m.sim == doctest::Approx(1.6f / (std::sqrt(10.f) * std::sqrt(1.04f))).epsilon(0.001));
  CHECK(FaceDb().best(axis(0), 0.5f).sim == 0.f);
}

TEST_CASE("samples capped at 20, oldest dropped") {
  FaceDb db;
  for (int i = 0; i < 5; i++) REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  auto l = db.list();
  REQUIRE(l.size() == 1);
  CHECK(l[0].second == 20);
}

TEST_CASE("16 names max -> kFull") {
  FaceDb db;
  for (int i = 0; i < 16; i++) REQUIRE(db.add("p" + std::to_string(i), five(i * 7), 0.5f, nullptr) == AddResult::kOk);
  CHECK(db.add("extra", five(120), 0.5f, nullptr) == AddResult::kFull);
  CHECK(db.add("p3", five(21), 0.5f, nullptr) == AddResult::kOk);  // existing name still accepts samples
}

TEST_CASE("conflict with another name is rejected and nothing stored") {
  FaceDb db;
  REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  std::string other;
  CHECK(db.add("bob", five(0), 0.5f, &other) == AddResult::kConflict);
  CHECK(other == "alice");
  CHECK(db.list().size() == 1);
  CHECK(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);  // same name is fine
}

TEST_CASE("valid_name") {
  CHECK_FALSE(FaceDb::valid_name(""));
  CHECK_FALSE(FaceDb::valid_name(std::string(33, 'a')));
  CHECK(FaceDb::valid_name(std::string(32, 'a')));
  CHECK_FALSE(FaceDb::valid_name("a\nb"));
  CHECK(FaceDb::valid_name("\xe5\xb0\x8f\xe6\x98\x8e"));  // 小明, 6 bytes
  FaceDb db;
  CHECK(db.add("", five(0), 0.5f, nullptr) == AddResult::kBadName);
}

TEST_CASE("forget and list") {
  FaceDb db;
  REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  CHECK_FALSE(db.forget("bob"));
  CHECK(db.forget("alice"));
  CHECK(db.list().empty());
}

TEST_CASE("serialize/parse round trip") {
  FaceDb db;
  REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  REQUIRE(db.add("\xe5\xb0\x8f\xe6\x98\x8e", five(30), 0.5f, nullptr) == AddResult::kOk);
  auto p = FaceDb::parse(db.serialize());
  REQUIRE(p);
  CHECK(p->list() == db.list());
  CHECK(p->best(axis(30), 0.5f).name == "\xe5\xb0\x8f\xe6\x98\x8e");
  CHECK(FaceDb::parse(FaceDb().serialize()));
}

TEST_CASE("parse rejects bad magic, bad CRC, truncated file") {
  FaceDb db;
  REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  const std::string s = db.serialize();
  std::string bad = s;
  bad[0] = 'X';
  CHECK_FALSE(FaceDb::parse(bad));
  bad = s;
  bad[s.size() / 2] ^= 0x40;
  CHECK_FALSE(FaceDb::parse(bad));
  CHECK_FALSE(FaceDb::parse(s.substr(0, s.size() - 1)));
  CHECK_FALSE(FaceDb::parse(""));
}

TEST_CASE("save is atomic and load reads it back") {
  const std::string d = tmpdir(), p = d + "/faces.bin";
  FaceDb db;
  REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  REQUIRE(db.save(p));
  CHECK(ls(d) == std::vector<std::string>{"faces.bin"});
  std::string note;
  FaceDb back = FaceDb::load(p, &note);
  CHECK(back.list() == db.list());
  CHECK(note.empty());
  std::remove(p.c_str());
  rmdir(d.c_str());
}

TEST_CASE("load missing file gives empty db without a note") {
  std::string note = "x";
  CHECK(FaceDb::load("/nonexistent-dir/faces.bin", &note).list().empty());
  CHECK(note.empty());
}

TEST_CASE("load corrupt file renames it to .bad and starts empty") {
  const std::string d = tmpdir(), p = d + "/faces.bin";
  FaceDb db;
  REQUIRE(db.add("alice", five(0), 0.5f, nullptr) == AddResult::kOk);
  REQUIRE(db.save(p));
  std::string s = slurp(p);
  spit(p, s.substr(0, s.size() - 3));  // truncated
  std::string note;
  FaceDb got = FaceDb::load(p, &note);
  CHECK(got.list().empty());
  CHECK_FALSE(note.empty());
  auto files = ls(d);
  REQUIRE(files.size() == 1);
  CHECK(files[0].rfind("faces.bin.bad.", 0) == 0);
  // next save writes a fresh library that loads fine
  REQUIRE(got.add("bob", five(10), 0.5f, nullptr) == AddResult::kOk);
  REQUIRE(got.save(p));
  CHECK(FaceDb::load(p, &note).list().size() == 1);
  for (auto& f : ls(d)) std::remove((d + "/" + f).c_str());
  rmdir(d.c_str());
}

TEST_CASE("an unreadable library is not treated as missing: it is quarantined, never overwritten") {
  // A path that exists but cannot be read as a file (here a directory, so it also fails for
  // root; in the field: permissions or EIO) must not be taken for "no library yet".
  const std::string d = tmpdir(), p = d + "/faces.bin";
  REQUIRE(mkdir(p.c_str(), 0700) == 0);
  std::string note;
  FaceDb got = FaceDb::load(p, &note);
  CHECK(got.list().empty());
  CHECK_FALSE(note.empty());
  struct stat st;
  CHECK(stat(p.c_str(), &st) != 0);  // moved away: the next save cannot overwrite it
  bool kept = false;
  for (auto& f : ls(d)) kept = kept || f.rfind("faces.bin.bad.", 0) == 0;
  CHECK(kept);
  REQUIRE(got.add("bob", five(10), 0.5f, nullptr) == AddResult::kOk);
  CHECK(got.save(p));
  for (auto& f : ls(d)) {
    const std::string q = d + "/" + f;
    if (rmdir(q.c_str()) != 0) std::remove(q.c_str());
  }
  rmdir(d.c_str());
}
