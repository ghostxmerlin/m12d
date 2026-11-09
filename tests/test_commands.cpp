#include <string>
#include "doctest.h"
#include "proto/commands.h"

namespace {
StatusSource test_source(bool have_snap = true) {
  StatusSource src;
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
  RuntimeCfg cfg;
  Dispatcher d{cfg, test_source()};
  bool sub = true;
  std::string h(const std::string& line, bool too_long = false) { return d.handle(line, too_long, sub); }
};
const char* kDefaultCfg = "\"cfg\":{\"det\":true,\"lm\":true,\"max_faces\":10,\"fps_cap\":0.0,\"sub\":true}";
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
        R"({"id":5,"ok":true,"cfg":{"det":false,"lm":false,"max_faces":3,"fps_cap":4.5,"sub":true}})");
  CHECK(t.cfg.max_faces == 3);
  CHECK(t.cfg.with_lm == false);
  CHECK(t.cfg.det_on == false);
  CHECK(t.cfg.fps_cap == doctest::Approx(4.5));
}

TEST_CASE("cfg sub") {
  D t;
  CHECK(t.h(R"({"id":6,"do":"cfg","sub":false})") ==
        R"({"id":6,"ok":true,"cfg":{"det":true,"lm":true,"max_faces":10,"fps_cap":0.0,"sub":false}})");
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
