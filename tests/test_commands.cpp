#include <string>
#include "doctest.h"
#include <vector>
#include "proto/commands.h"

namespace {
std::vector<FaceJob> g_jobs;
bool g_queue_full = false, g_paused = false;
StatusSource test_source(bool have_snap = true) {
  StatusSource src;
  src.face_job = [](FaceJob j) {
    if (g_queue_full) return false;
    g_jobs.push_back(std::move(j));
    return true;
  };
  src.paused = [] { return g_paused; };
  src.faceid_enabled = true;
  src.hb_body = [] { return std::string("{\"t\":70}"); };
  src.snap = [have_snap] {
    return have_snap ? std::string("\"f\":5,\"path\":\"/tmp/m12d_snap.yuyv\",\"num\":0,\"face\":[]") : std::string();
  };
  src.version = "0.1.0";
  src.git = "abc123";
  src.models = {"mnet.25-opt"};
  return src;
}
struct D {
  D() {
    g_jobs.clear();
    g_queue_full = g_paused = false;
  }
  RuntimeCfg cfg;
  Dispatcher d{cfg, test_source()};
  bool sub = true;
  std::string h(const std::string& line, bool too_long = false) { return d.handle(line, too_long, sub); }
};
const char* kDefaultCfg = "\"cfg\":{\"det\":true,\"lm\":true,\"attr\":true,\"person\":true,\"id_th\":0.50,\"max_faces\":10,\"fps_cap\":0.0,\"sub\":true}";
}  // namespace

TEST_CASE("ping") {
  D t;
  CHECK(t.h(R"({"id":1,"do":"ping"})") == R"({"id":1,"ok":true})");
}

TEST_CASE("ver") {
  D t;
  CHECK(t.h(R"({"id":2,"do":"ver"})") ==
        R"({"id":2,"ok":true,"ver":"0.1.0","git":"abc123","models":["mnet.25-opt"]})");
}

TEST_CASE("status") {
  D t;
  CHECK(t.h(R"({"id":3,"do":"status"})") == R"({"id":3,"ok":true,"hb":{"t":70}})");
}

TEST_CASE("cfg read") {
  D t;
  CHECK(t.h(R"({"id":4,"do":"cfg"})") == std::string("{\"id\":4,\"ok\":true,") + kDefaultCfg + "}");
}

TEST_CASE("cfg write") {
  D t;
  CHECK(t.h(R"({"id":5,"do":"cfg","max_faces":3,"lm":false,"fps_cap":4.5,"det":false})") ==
        R"({"id":5,"ok":true,"cfg":{"det":false,"lm":false,"attr":true,"person":true,"id_th":0.50,"max_faces":3,"fps_cap":4.5,"sub":true}})");
  CHECK(t.cfg.max_faces == 3);
  CHECK(t.cfg.with_lm == false);
  CHECK(t.cfg.det_on == false);
  CHECK(t.cfg.fps_cap == doctest::Approx(4.5));
}

TEST_CASE("cfg attr") {
  D t;
  CHECK(t.h(R"({"id":30,"do":"cfg","attr":false})") ==
        R"({"id":30,"ok":true,"cfg":{"det":true,"lm":true,"attr":false,"person":true,"id_th":0.50,"max_faces":10,"fps_cap":0.0,"sub":true}})");
  CHECK(t.cfg.attr_on == false);
  CHECK(t.h(R"({"id":31,"do":"cfg","attr":1})") == R"({"id":31,"ok":false,"err":"badarg"})");
}

TEST_CASE("cfg sub") {
  D t;
  CHECK(t.h(R"({"id":6,"do":"cfg","sub":false})") ==
        R"({"id":6,"ok":true,"cfg":{"det":true,"lm":true,"attr":true,"person":true,"id_th":0.50,"max_faces":10,"fps_cap":0.0,"sub":false}})");
  CHECK(t.sub == false);
}

TEST_CASE("cfg with one bad arg applies nothing") {
  D t;
  CHECK(t.h(R"({"id":7,"do":"cfg","max_faces":3,"fps_cap":99})") == R"({"id":7,"ok":false,"err":"badarg"})");
  CHECK(t.cfg.max_faces == 10);
  CHECK(t.h(R"({"id":8,"do":"cfg","lm":false,"wat":1})") == R"({"id":8,"ok":false,"err":"badarg"})");
  CHECK(t.cfg.with_lm == true);
  CHECK(t.h(R"({"id":9,"do":"cfg","max_faces":2.5})") == R"({"id":9,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":10,"do":"cfg","max_faces":0})") == R"({"id":10,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":11,"do":"cfg","det":"yes"})") == R"({"id":11,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":12,"do":"cfg","sub":false,"fps_cap":-1})") == R"({"id":12,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":16,"do":"cfg","fps_cap":0.1})") == R"({"id":16,"ok":false,"err":"badarg"})");  // below 0.5
  CHECK(t.h(R"({"id":17,"do":"cfg","fps_cap":1e-30})") == R"({"id":17,"ok":false,"err":"badarg"})");
  CHECK(t.sub == true);
  CHECK(t.h(R"({"id":13,"do":"cfg"})") == std::string("{\"id\":13,\"ok\":true,") + kDefaultCfg + "}");
}

TEST_CASE("snap") {
  D t;
  CHECK(t.h(R"({"id":14,"do":"snap"})") ==
        R"({"id":14,"ok":true,"f":5,"path":"/tmp/m12d_snap.yuyv","num":0,"face":[]})");
  RuntimeCfg cfg;
  Dispatcher none(cfg, test_source(false));
  bool sub = true;
  CHECK(none.handle(R"({"id":15,"do":"snap"})", false, sub) == R"({"id":15,"ok":false,"err":"noface"})");
}

TEST_CASE("errors") {
  D t;
  CHECK(t.h("not json") == R"({"id":-1,"ok":false,"err":"badjson"})");
  CHECK(t.h("[1]") == R"({"id":-1,"ok":false,"err":"badjson"})");
  CHECK(t.h(R"({"do":"ping"})") == R"({"id":-1,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":"1","do":"ping"})") == R"({"id":-1,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":1.5,"do":"ping"})") == R"({"id":-1,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":-3,"do":"ping"})") == R"({"id":-1,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":2147483648,"do":"ping"})") == R"({"id":-1,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":2147483647,"do":"ping"})") == R"({"id":2147483647,"ok":true})");
  CHECK(t.h(R"({"id":20})") == R"({"id":20,"ok":false,"err":"unknown"})");
  CHECK(t.h(R"({"id":21,"do":"fly"})") == R"({"id":21,"ok":false,"err":"unknown"})");
  CHECK(t.h(R"({"id":22,"do":7})") == R"({"id":22,"ok":false,"err":"unknown"})");
  CHECK(t.h(R"({"id":23,"do":"ping","x":1})") == R"({"id":23,"ok":false,"err":"badarg"})");
  CHECK(t.h("", true) == R"({"id":-1,"ok":false,"err":"toolong"})");
}

TEST_CASE("cfg person") {
  D t;
  CHECK(t.cfg.person_on == true);
  CHECK(t.h(R"({"id":31,"do":"cfg","person":false})") ==
        R"({"id":31,"ok":true,"cfg":{"det":true,"lm":true,"attr":true,"person":false,"id_th":0.50,"max_faces":10,"fps_cap":0.0,"sub":true}})");
  CHECK(t.cfg.person_on == false);
  CHECK(t.h(R"({"id":32,"do":"cfg","person":1})") == R"({"id":32,"ok":false,"err":"badarg"})");
  CHECK(t.cfg.person_on == false);
}

TEST_CASE("cfg id_th") {
  D t;
  CHECK(t.h(R"({"id":40,"do":"cfg","id_th":0.62})").find("\"id_th\":0.62") != std::string::npos);
  CHECK(t.cfg.id_th == doctest::Approx(0.62f));
  CHECK(t.h(R"({"id":41,"do":"cfg","id_th":0.95})") == R"({"id":41,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":42,"do":"cfg","id_th":0.29})") == R"({"id":42,"ok":false,"err":"badarg"})");
  CHECK(t.cfg.id_th == doctest::Approx(0.62f));
}

TEST_CASE("learn / who / forget / faces are queued with no immediate reply") {
  D t;
  CHECK(t.h(R"({"id":50,"do":"learn","name":"alice"})").empty());
  CHECK(t.h(R"({"id":51,"do":"who","face":3})").empty());
  CHECK(t.h(R"({"id":52,"do":"forget","name":"alice"})").empty());
  CHECK(t.h(R"({"id":53,"do":"faces"})").empty());
  REQUIRE(g_jobs.size() == 4);
  CHECK(g_jobs[0].kind == FaceJob::kLearn);
  CHECK(g_jobs[0].name == "alice");
  CHECK(g_jobs[0].req_id == 50);
  CHECK(g_jobs[1].kind == FaceJob::kWho);
  CHECK(g_jobs[1].face_id == 3);
  CHECK(g_jobs[2].kind == FaceJob::kForget);
  CHECK(g_jobs[3].kind == FaceJob::kList);
}

TEST_CASE("face-id commands: standby, hot, busy, nomodel, badarg") {
  D t;
  t.cfg.det_on = false;
  CHECK(t.h(R"({"id":60,"do":"who"})") == R"({"id":60,"ok":false,"err":"standby"})");
  CHECK(t.h(R"({"id":61,"do":"learn","name":"a"})") == R"({"id":61,"ok":false,"err":"standby"})");
  CHECK(t.h(R"({"id":62,"do":"faces"})").empty());  // no inference: still served in standby
  t.cfg.det_on = true;
  g_paused = true;
  CHECK(t.h(R"({"id":63,"do":"who"})") == R"({"id":63,"ok":false,"err":"hot"})");
  g_paused = false;
  g_queue_full = true;
  CHECK(t.h(R"({"id":64,"do":"who"})") == R"({"id":64,"ok":false,"err":"busy"})");
  g_queue_full = false;
  CHECK(t.h(R"({"id":65,"do":"learn","name":""})") == R"({"id":65,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":66,"do":"learn","name":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"})") == R"({"id":66,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":67,"do":"learn"})") == R"({"id":67,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":68,"do":"who","face":0})") == R"({"id":68,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":69,"do":"who","face":"x"})") == R"({"id":69,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":70,"do":"who","extra":1})") == R"({"id":70,"ok":false,"err":"badarg"})");
  CHECK(t.h(R"({"id":71,"do":"forget"})") == R"({"id":71,"ok":false,"err":"badarg"})");
}

TEST_CASE("face-id commands without the model reply nomodel") {
  RuntimeCfg cfg;
  StatusSource src = test_source();
  src.faceid_enabled = false;
  Dispatcher d{cfg, src};
  bool sub = true;
  CHECK(d.handle(R"({"id":80,"do":"who"})", false, sub) == R"({"id":80,"ok":false,"err":"nomodel"})");
  CHECK(d.handle(R"({"id":81,"do":"faces"})", false, sub) == R"({"id":81,"ok":false,"err":"nomodel"})");
}
