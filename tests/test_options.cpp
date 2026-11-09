#include <string>
#include <vector>
#include "app/options.h"
#include "doctest.h"

namespace {
std::optional<Options> parse(std::vector<std::string> args, std::string& err) {
  std::vector<char*> argv;
  static std::string prog = "m12d";
  argv.push_back(prog.data());
  for (auto& a : args) argv.push_back(a.data());
  return parse_args(static_cast<int>(argv.size()), argv.data(), err);
}
}  // namespace

TEST_CASE("defaults") {
  std::string err;
  auto o = parse({}, err);
  REQUIRE(o);
  CHECK(o->models == "models");
  CHECK(o->serial == "/dev/ttyS1");
  CHECK(o->video == "/dev/video0");
  CHECK(o->use_serial);
  CHECK_FALSE(o->use_stdout);
  CHECK(o->tcp_port == 5555);
  CHECK(o->tcp_bind == "10.254.239.1");
  CHECK(o->fps_cap == 0);
  CHECK(o->cap_fps == 15);
  CHECK(o->max_faces == 10);
  CHECK(o->threads == 2);
  CHECK(o->hb_ms == 2000);
}

TEST_CASE("flags") {
  std::string err;
  auto o = parse({"--no-serial", "--stdout", "--fps-cap", "4", "--max-faces", "3", "--tcp-port", "0", "--tcp-bind",
                  "127.0.0.1", "--models", "/m", "--serial", "/dev/ttyX", "--cap-fps", "10", "--threads", "1",
                  "--hb-ms", "500"},
                 err);
  REQUIRE(o);
  CHECK_FALSE(o->use_serial);
  CHECK(o->use_stdout);
  CHECK(o->fps_cap == doctest::Approx(4));
  CHECK(o->max_faces == 3);
  CHECK(o->tcp_port == 0);
  CHECK(o->tcp_bind == "127.0.0.1");
  CHECK(o->models == "/m");
  CHECK(o->serial == "/dev/ttyX");
  CHECK(o->cap_fps == 10);
  CHECK(o->threads == 1);
  CHECK(o->hb_ms == 500);
}

TEST_CASE("bad value") {
  for (auto args : std::vector<std::vector<std::string>>{{"--fps-cap", "abc"},
                                                         {"--tcp-port", "70000"},
                                                         {"--max-faces", "0"},
                                                         {"--max-faces", "11"},
                                                         {"--fps-cap", "31"},
                                                         {"--threads", "0"},
                                                         {"--hb-ms", "10"},
                                                         {"--cap-fps", "4x"},
                                                         {"--models"}}) {
    std::string err;
    CHECK_FALSE(parse(args, err));
    CHECK_FALSE(err.empty());
  }
}

TEST_CASE("unknown flag") {
  std::string err;
  CHECK_FALSE(parse({"--wat"}, err));
  CHECK_FALSE(err.empty());
}
