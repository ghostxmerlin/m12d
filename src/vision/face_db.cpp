#include "vision/face_db.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>

#include "sys/fileio.h"

namespace {
constexpr char kMagic[4] = {'M', '1', '2', 'F'};
constexpr uint32_t kVersion = 1;

uint32_t crc32(const std::string& s, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= static_cast<uint8_t>(s[i]);
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}
void put_u32(std::string& s, uint32_t v) {
  for (int i = 0; i < 4; i++) s.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
bool get_u32(const std::string& s, size_t& off, size_t end, uint32_t& v) {
  if (end - off < 4) return false;
  v = 0;
  for (int i = 0; i < 4; i++) v |= static_cast<uint32_t>(static_cast<uint8_t>(s[off + i])) << (8 * i);
  off += 4;
  return true;
}
}  // namespace

bool FaceDb::valid_name(const std::string& n) {
  if (n.empty() || n.size() > kMaxNameBytes) return false;
  for (unsigned char c : n)
    if (c < 0x20 || c == 0x7F) return false;
  return true;
}

AddResult FaceDb::add(const std::string& name, const std::vector<Embedding>& samples, float th,
                      std::string* conflict_name) {
  if (!valid_name(name)) return AddResult::kBadName;
  for (const Person& p : people_) {
    if (p.name == name) continue;
    for (const Embedding& s : samples)
      for (const Embedding& q : p.samples)
        if (cosine(s, q) >= th) {
          if (conflict_name) *conflict_name = p.name;
          return AddResult::kConflict;
        }
  }
  auto it = std::find_if(people_.begin(), people_.end(), [&](const Person& p) { return p.name == name; });
  if (it == people_.end()) {
    if (people_.size() >= kMaxNames) return AddResult::kFull;
    people_.push_back(Person{name, {}});
    it = people_.end() - 1;
  }
  for (const Embedding& s : samples) {
    it->samples.push_back(s);
    if (it->samples.size() > kMaxSamples) it->samples.pop_front();
  }
  return AddResult::kOk;
}

Match FaceDb::best(const Embedding& e, float th) const {
  Match m{"", 0.f};
  bool any = false;
  for (const Person& p : people_)
    for (const Embedding& q : p.samples) {
      const float c = cosine(e, q);
      if (!any || c > m.sim) {
        m.sim = c;
        m.name = p.name;
        any = true;
      }
    }
  if (!any || m.sim < th) m.name.clear();
  return m;
}

bool FaceDb::forget(const std::string& name) {
  auto it = std::find_if(people_.begin(), people_.end(), [&](const Person& p) { return p.name == name; });
  if (it == people_.end()) return false;
  people_.erase(it);
  return true;
}

std::vector<std::pair<std::string, size_t>> FaceDb::list() const {
  std::vector<std::pair<std::string, size_t>> out;
  for (const Person& p : people_) out.emplace_back(p.name, p.samples.size());
  return out;
}

std::string FaceDb::serialize() const {
  std::string s(kMagic, 4);
  put_u32(s, kVersion);
  put_u32(s, static_cast<uint32_t>(people_.size()));
  for (const Person& p : people_) {
    put_u32(s, static_cast<uint32_t>(p.name.size()));
    s += p.name;
    put_u32(s, static_cast<uint32_t>(p.samples.size()));
    for (const Embedding& e : p.samples) {
      static_assert(sizeof(float) == 4, "f32");
      for (float v : e) {
        uint32_t u;
        std::memcpy(&u, &v, 4);
        put_u32(s, u);
      }
    }
  }
  put_u32(s, crc32(s, s.size()));
  return s;
}

std::optional<FaceDb> FaceDb::parse(const std::string& b) {
  if (b.size() < 16 || std::memcmp(b.data(), kMagic, 4) != 0) return std::nullopt;
  const size_t end = b.size() - 4;
  size_t off = end;
  uint32_t crc = 0, ver = 0, n = 0;
  if (!get_u32(b, off, b.size(), crc) || crc != crc32(b, end)) return std::nullopt;
  off = 4;
  if (!get_u32(b, off, end, ver) || ver != kVersion || !get_u32(b, off, end, n) || n > kMaxNames) return std::nullopt;
  FaceDb db;
  for (uint32_t i = 0; i < n; i++) {
    uint32_t len = 0, cnt = 0;
    if (!get_u32(b, off, end, len) || len > kMaxNameBytes || end - off < len) return std::nullopt;
    Person p{b.substr(off, len), {}};
    off += len;
    if (!valid_name(p.name) || !get_u32(b, off, end, cnt) || cnt == 0 || cnt > kMaxSamples) return std::nullopt;
    for (uint32_t k = 0; k < cnt; k++) {
      Embedding e;
      for (float& v : e) {
        uint32_t u;
        if (!get_u32(b, off, end, u)) return std::nullopt;
        std::memcpy(&v, &u, 4);
      }
      p.samples.push_back(e);
    }
    db.people_.push_back(std::move(p));
  }
  if (off != end) return std::nullopt;
  return db;
}

bool FaceDb::save(const std::string& path) const {
  const std::string s = serialize();
  return write_file_private(path, s.data(), s.size(), true);
}

FaceDb FaceDb::load(const std::string& path, std::string* note) {
  if (note) note->clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) return FaceDb();
  const std::string bytes((std::istreambuf_iterator<char>(f)), {});
  f.close();
  if (auto db = parse(bytes)) return *db;
  const std::string bad = path + ".bad." + std::to_string(static_cast<long long>(std::time(nullptr)));
  const bool moved = std::rename(path.c_str(), bad.c_str()) == 0;
  if (note) *note = "face library " + path + " is corrupt; " + (moved ? "moved to " + bad : "could not move it") + "; starting empty";
  return FaceDb();
}
