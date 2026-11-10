#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "vision/face_id_worker.h"

// Runtime configuration: written by the dispatcher (IO thread), read by the detect thread.
struct RuntimeCfg {
  std::atomic<bool> det_on{true}, with_lm{true}, attr_on{true}, person_on{true};
  std::atomic<float> id_th{0.5f};  // face-ID cosine threshold (phase 2), 0.3..0.9
  std::atomic<int> max_faces{10};
  std::atomic<float> fps_cap{0};
};

struct StatusSource {
  std::function<std::string()> hb_body;  // heartbeat object, same as in the hb push line
  std::function<std::string()> snap;     // JSON members (no braces) for the snap reply; "" = no frame yet
  std::function<bool(FaceJob)> face_job;  // queue a learn/who/forget/faces job; false = queue full
  std::function<bool()> paused;           // thermal pause in effect
  bool faceid_enabled = false;            // mobilefacenet loaded
  std::string version, git;
  std::vector<std::string> models;
};

// Unified command set shared by serial and TCP (design §6). One request line in,
// one reply line out (without '\n'). Phase-1 commands: ping, ver, status, cfg, snap.
class Dispatcher {
 public:
  Dispatcher(RuntimeCfg& cfg, StatusSource src);
  // sub: this channel's push subscription; cfg may change it.
  // chan: the requesting channel, recorded in queued face-ID jobs for the deferred reply.
  std::string handle(std::string_view line, bool too_long, bool& sub, int chan = 0);

 private:
  RuntimeCfg& cfg_;
  StatusSource src_;
};
