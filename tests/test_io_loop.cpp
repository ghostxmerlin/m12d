#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>
#include "doctest.h"
#include "io/io_loop.h"
#include "io/serial_port.h"
#include "io/tcp_listener.h"
#include "io_test_util.h"

using namespace std::chrono;

namespace {
// Runs an IoLoop on a pty "serial port" + an ephemeral TCP port. Handler echoes "R:<line>";
// the line "unsub" unsubscribes the sender; too-long lines reply "TOOLONG".
struct Rig {
  Pty pty;
  int port = 0;
  std::atomic<int> handled{0};
  IoLoop* loop = nullptr;
  std::atomic<bool> stop{false};
  std::thread th;
  explicit Rig(size_t max_clients = 2) {
    int sfd = open_serial(pty.slave_path, 115200);
    REQUIRE(sfd >= 0);
    int lfd = open_tcp_listener(0, 4);
    REQUIRE(lfd >= 0);
    port = listen_port(lfd);
    loop = new IoLoop(sfd, lfd, max_clients, [this](ChanId c, std::string_view line, bool too_long) -> std::string {
      handled++;
      if (too_long) return "TOOLONG";
      if (line == "unsub") {
        loop->set_subscribed(c, false);
        return "ok";
      }
      return "R:" + std::string(line);
    });
    th = std::thread([this] { loop->run(stop); });
  }
  ~Rig() {
    stop = true;
    th.join();
    delete loop;
  }
};
}  // namespace

TEST_CASE("serial reply stays on serial") {
  Rig r;
  int c = tcp_connect(r.port);
  REQUIRE(c >= 0);
  write_all(r.pty.master, "a\n");
  CHECK(read_lines(r.pty.master, 1, 1000) == "R:a\n");
  CHECK(read_for(c, 200).empty());
  close(c);
}

TEST_CASE("tcp reply stays on tcp") {
  Rig r;
  int c = tcp_connect(r.port);
  REQUIRE(c >= 0);
  write_all(c, "b\n");
  CHECK(read_lines(c, 1, 1000) == "R:b\n");
  CHECK(read_for(r.pty.master, 200).empty());
  close(c);
}

TEST_CASE("push to all subscribed") {
  Rig r;
  int c = tcp_connect(r.port);
  REQUIRE(c >= 0);
  // make sure the client is registered before pushing
  write_all(c, "hi\n");
  REQUIRE(read_lines(c, 1, 1000) == "R:hi\n");
  r.loop->push("P");
  CHECK(read_lines(r.pty.master, 1, 1000) == "P\n");
  CHECK(read_lines(c, 1, 1000) == "P\n");
  write_all(c, "unsub\n");
  REQUIRE(read_lines(c, 1, 1000) == "ok\n");
  r.loop->push("Q");
  CHECK(read_lines(r.pty.master, 1, 1000) == "Q\n");
  CHECK(read_for(c, 200).empty());
  close(c);
}

TEST_CASE("third client rejected") {
  Rig r(2);
  int a = tcp_connect(r.port), b = tcp_connect(r.port);
  REQUIRE(a >= 0);
  REQUIRE(b >= 0);
  write_all(a, "1\n");
  write_all(b, "2\n");
  REQUIRE(read_lines(a, 1, 1000) == "R:1\n");
  REQUIRE(read_lines(b, 1, 1000) == "R:2\n");
  int c = tcp_connect(r.port);
  REQUIRE(c >= 0);
  bool eof = false;
  CHECK(read_for(c, 1000, SIZE_MAX, &eof) == "{\"id\":-1,\"ok\":false,\"err\":\"busy\"}\n");
  CHECK(eof);
  CHECK(r.loop->stats().rejected_clients == 1);
  write_all(a, "3\n");
  CHECK(read_lines(a, 1, 1000) == "R:3\n");
  close(a);
  close(b);
  close(c);
}

TEST_CASE("slow tcp reader does not block") {
  Rig r;
  int c = tcp_connect(r.port, 4096);  // never reads; tiny receive buffer
  REQUIRE(c >= 0);
  write_all(c, "hi\n");
  // drain serial continuously in the background
  std::atomic<bool> done{false};
  std::string serial_rx;
  std::thread reader([&] {
    while (!done) serial_rx += read_for(r.pty.master, 50);
    serial_rx += read_for(r.pty.master, 300);
  });
  std::this_thread::sleep_for(milliseconds(100));
  const std::string payload(199, 'x');
  auto t0 = steady_clock::now();
  for (int i = 0; i < 5000; i++) r.loop->push(payload);
  // Give the loop time to try to deliver everything.
  std::this_thread::sleep_for(milliseconds(500));
  auto ms = duration_cast<milliseconds>(steady_clock::now() - t0).count();
  done = true;
  reader.join();
  CHECK(ms <= 2000);
  CHECK(r.loop->stats().dropped > 0);
  // every line that reached the serial side is complete
  std::istringstream in(serial_rx);
  std::string line;
  int lines = 0;
  while (std::getline(in, line)) {
    if (line == "R:hi") continue;
    CHECK(line == payload);
    lines++;
  }
  CHECK(lines > 0);
  CHECK(serial_rx.back() == '\n');
  close(c);
}

TEST_CASE("disconnect mid-line") {
  Rig r;
  int c = tcp_connect(r.port);
  REQUIRE(c >= 0);
  write_all(c, "abc");
  close(c);
  std::this_thread::sleep_for(milliseconds(200));
  CHECK(r.handled == 0);
  // loop still serves serial
  write_all(r.pty.master, "x\n");
  CHECK(read_lines(r.pty.master, 1, 1000) == "R:x\n");
}

TEST_CASE("open_serial fails on missing path") { CHECK(open_serial("/nonexistent", 115200) == -1); }

TEST_CASE("stop exits") {
  auto* r = new Rig();
  std::this_thread::sleep_for(milliseconds(50));
  auto t0 = steady_clock::now();
  delete r;  // sets stop and joins
  CHECK(duration_cast<milliseconds>(steady_clock::now() - t0).count() < 300);
}

// ---- Unified command set: the same script over serial (pty) and TCP ----
#include <fstream>
#include <vector>
#include "proto/commands.h"
#include "proto/det_json.h"

namespace {
// Script lines: raw request, "@crlf <req>" (sent with \r\n) or "@long N" (N junk bytes).
std::vector<std::string> load_script(const char* path) {
  std::ifstream f(path);
  std::vector<std::string> out;
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("@crlf ", 0) == 0)
      out.push_back(line.substr(6) + "\r\n");
    else if (line.rfind("@long ", 0) == 0)
      out.push_back(std::string(std::stoul(line.substr(6)), 'a') + "\n");
    else if (!line.empty())
      out.push_back(line + "\n");
  }
  return out;
}
std::vector<std::string> load_lines(const char* path) {
  std::ifstream f(path);
  std::vector<std::string> out;
  std::string line;
  while (std::getline(f, line))
    if (!line.empty()) out.push_back(line);
  return out;
}
std::vector<std::string> run_script(int fd, const std::vector<std::string>& script) {
  std::vector<std::string> replies;
  for (const std::string& req : script) {
    write_all(fd, req);
    std::string r = read_lines(fd, 1, 2000);
    if (!r.empty() && r.back() == '\n') r.pop_back();
    replies.push_back(r);
  }
  return replies;
}
}  // namespace

TEST_CASE("same replies over serial and tcp") {
  const auto script = load_script("tests/cmd_script.jsonl");
  const auto expected = load_lines("tests/cmd_expected.jsonl");
  REQUIRE(script.size() == expected.size());

  Pty pty;
  int sfd = open_serial(pty.slave_path, 115200);
  int lfd = open_tcp_listener(0, 4);
  REQUIRE(sfd >= 0);
  REQUIRE(lfd >= 0);
  const int port = listen_port(lfd);
  RuntimeCfg cfg;
  StatusSource src;
  src.hb_body = [] { return hb_body(70, 6.8f, 0, 1, 70000, 1, 0, 0, 1400); };
  src.version = "0.1.0";
  src.git = "test";
  src.models = {"mnet.25-opt"};
  Dispatcher disp(cfg, src);
  IoLoop* loop = nullptr;
  loop = new IoLoop(sfd, lfd, 2, [&](ChanId c, std::string_view line, bool too_long) {
    bool sub = loop->subscribed(c);
    std::string r = disp.handle(line, too_long, sub);
    loop->set_subscribed(c, sub);
    return r;
  });
  std::atomic<bool> stop{false};
  std::thread th([&] { loop->run(stop); });

  auto serial_replies = run_script(pty.master, script);
  int c = tcp_connect(port);
  REQUIRE(c >= 0);
  auto tcp_replies = run_script(c, script);
  close(c);
  stop = true;
  th.join();
  delete loop;

  CHECK(serial_replies == tcp_replies);
  for (size_t i = 0; i < expected.size(); i++) {
    INFO("script line " << i + 1);
    CHECK(serial_replies[i] == expected[i]);
  }
}
