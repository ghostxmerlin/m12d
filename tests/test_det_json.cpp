#include <string>
#include <vector>
#include "doctest.h"
#include "proto/det_json.h"

static FaceBox face(float x, float y, float w, float h, float prob) {
  FaceBox b{x, y, w, h, prob, {}};
  const float lm[5][2] = {{11.7f, 21.2f}, {15.2f, 21.9f}, {13.0f, 25.5f}, {12.1f, 29.9f}, {16.8f, 29.1f}};
  for (int k = 0; k < 5; k++) {
    b.lm[k][0] = lm[k][0];
    b.lm[k][1] = lm[k][1];
  }
  return b;
}

TEST_CASE("stock-compatible face line") {
  std::vector<FaceBox> faces = {face(10, 20, 30, 40, 0.98765f)};
  CHECK(det_json(7, faces, 2.0f, 10) ==
        "{\"f\":7,\"num\":1,\"face\":[{\"x\":20.0,\"y\":40.0,\"w\":60.0,\"h\":80.0,\"prob\":0.988,"
        "\"mark\":[{\"x\":22,\"y\":42},{\"x\":30,\"y\":42},{\"x\":26,\"y\":50},{\"x\":24,\"y\":58},"
        "{\"x\":32,\"y\":58}]}],\"running\":\"Face Detector\"}");
}

TEST_CASE("zero faces still emitted") {
  CHECK(det_json(8, {}, 2.0f, 10) == "{\"f\":8,\"num\":0,\"face\":[],\"running\":\"Face Detector\"}");
}

TEST_CASE("caps to max_faces by prob") {
  std::vector<FaceBox> faces;
  for (int i = 0; i < 12; i++) faces.push_back(face(i, 0, 10, 10, 0.80f + 0.01f * ((i * 7) % 12)));
  std::string s = det_json(1, faces, 1.0f, 10);
  CHECK(s.find("\"num\":10,") != std::string::npos);
  // probs are 0.80 + 0.01*k for k = 0..11; the two lowest (0.800, 0.810) must be gone
  CHECK(s.find("\"prob\":0.800") == std::string::npos);
  CHECK(s.find("\"prob\":0.810") == std::string::npos);
  CHECK(s.find("\"prob\":0.910") != std::string::npos);
  CHECK(s.find("\"prob\":0.820") != std::string::npos);
  // highest first
  CHECK(s.find("\"prob\":0.910") < s.find("\"prob\":0.900"));
}

TEST_CASE("line length bound") {
  std::vector<FaceBox> faces;
  for (int i = 0; i < 10; i++) {
    FaceBox b = face(319.9f, 239.9f, 319.9f, 239.9f, 0.999f);
    for (auto& p : b.lm) {
      p[0] = 319.9f;
      p[1] = 239.9f;
    }
    faces.push_back(b);
  }
  CHECK(det_json(18446744073709551615ull, faces, 2.0f, 10).size() < 2000);
}

TEST_CASE("lm off omits mark") {
  std::vector<FaceBox> faces = {face(10, 20, 30, 40, 0.98765f)};
  CHECK(det_json(7, faces, 2.0f, 10, false) ==
        "{\"f\":7,\"num\":1,\"face\":[{\"x\":20.0,\"y\":40.0,\"w\":60.0,\"h\":80.0,\"prob\":0.988}],"
        "\"running\":\"Face Detector\"}");
}

TEST_CASE("heartbeat") {
  CHECK(hb_json(78, 6.8f, 0, 123, 70000, 1000, 0, 0, 1400) ==
        "{\"hb\":{\"t\":78,\"fps\":6.8,\"tier\":0,\"up\":123,\"mem\":70000,\"frames\":1000,\"reopens\":0,"
        "\"drop\":0,\"mhz\":1400}}");
  CHECK(hb_body(78, 6.8f, 0, 123, 70000, 1000, 0, 0, 1400) ==
        "{\"t\":78,\"fps\":6.8,\"tier\":0,\"up\":123,\"mem\":70000,\"frames\":1000,\"reopens\":0,"
        "\"drop\":0,\"mhz\":1400}");
}

TEST_CASE("snap members") {
  std::vector<FaceBox> faces = {face(10, 20, 30, 40, 0.98765f)};
  CHECK(snap_members(9, "/tmp/m12d_snap.yuyv", faces, 2.0f, 10, false) ==
        "\"f\":9,\"path\":\"/tmp/m12d_snap.yuyv\",\"num\":1,\"face\":[{\"x\":20.0,\"y\":40.0,\"w\":60.0,\"h\":80.0,"
        "\"prob\":0.988}]");
  CHECK(snap_members(3, "/tmp/s", {}, 2.0f, 10) == "\"f\":3,\"path\":\"/tmp/s\",\"num\":0,\"face\":[]");
}

TEST_CASE("face extras: id and attributes appended after stock fields") {
  std::vector<FaceBox> faces = {face(10, 20, 30, 40, 0.98765f), face(50, 20, 30, 40, 0.5f)};
  std::vector<FaceExtra> extra(2);
  extra[0].id = 3;
  extra[0].attr.have = true;
  extra[0].attr.gender = 1;
  extra[0].attr.age = 45;
  extra[1].id = 4;  // no attributes yet
  std::string s = det_json(7, faces, 2.0f, 10, false, &extra);
  CHECK(s ==
        "{\"f\":7,\"num\":2,\"face\":["
        "{\"x\":20.0,\"y\":40.0,\"w\":60.0,\"h\":80.0,\"prob\":0.988,\"id\":3,\"gender\":\"m\",\"age\":45,"
        "\"age_group\":\"adult\"},"
        "{\"x\":100.0,\"y\":40.0,\"w\":60.0,\"h\":80.0,\"prob\":0.500,\"id\":4}"
        "],\"running\":\"Face Detector\"}");
}

TEST_CASE("face extras follow their face through the prob sort") {
  std::vector<FaceBox> faces = {face(10, 20, 30, 40, 0.5f), face(50, 20, 30, 40, 0.9f)};
  std::vector<FaceExtra> extra(2);
  extra[0].id = 1;
  extra[1].id = 2;
  extra[1].attr.have = true;
  extra[1].attr.gender = -1;
  extra[1].attr.age = 8;
  std::string s = det_json(1, faces, 1.0f, 10, false, &extra);
  CHECK(s.find("\"prob\":0.900,\"id\":2,\"gender\":\"?\",\"age\":8,\"age_group\":\"child\"") != std::string::npos);
  CHECK(s.find("\"prob\":0.500,\"id\":1}") != std::string::npos);
}
