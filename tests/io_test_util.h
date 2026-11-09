#pragma once
// Helpers for transport tests: a pty pair standing in for the serial port, and TCP clients.
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <string>

struct Pty {
  int master = -1;
  std::string slave_path;
  Pty() {
    master = posix_openpt(O_RDWR | O_NOCTTY);
    grantpt(master);
    unlockpt(master);
    slave_path = ptsname(master);
  }
  ~Pty() {
    if (master >= 0) close(master);
  }
};

inline int listen_port(int listen_fd) {
  sockaddr_in a{};
  socklen_t len = sizeof a;
  getsockname(listen_fd, reinterpret_cast<sockaddr*>(&a), &len);
  return ntohs(a.sin_port);
}

inline int tcp_connect(int port, int rcvbuf = 0) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (rcvbuf > 0) setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

inline void write_all(int fd, const std::string& s) {
  size_t off = 0;
  while (off < s.size()) {
    ssize_t n = write(fd, s.data() + off, s.size() - off);
    if (n <= 0) return;
    off += n;
  }
}

// Read until `want` bytes have arrived, EOF, or timeout. Returns what was read.
inline std::string read_for(int fd, int timeout_ms, size_t want = SIZE_MAX, bool* eof = nullptr) {
  using namespace std::chrono;
  std::string out;
  auto end = steady_clock::now() + milliseconds(timeout_ms);
  while (out.size() < want) {
    int left = static_cast<int>(duration_cast<milliseconds>(end - steady_clock::now()).count());
    if (left <= 0) break;
    pollfd p{fd, POLLIN, 0};
    if (poll(&p, 1, left) <= 0) break;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof buf);
    if (n <= 0) {
      if (eof) *eof = true;
      break;
    }
    out.append(buf, n);
  }
  return out;
}

// Read until the accumulated text ends with '\n' and contains n lines, or timeout.
inline std::string read_lines(int fd, int n, int timeout_ms) {
  using namespace std::chrono;
  std::string out;
  auto end = steady_clock::now() + milliseconds(timeout_ms);
  int count = 0;
  while (count < n) {
    int left = static_cast<int>(duration_cast<milliseconds>(end - steady_clock::now()).count());
    if (left <= 0) break;
    std::string got = read_for(fd, left, 1);
    if (got.empty()) break;
    for (char c : got) count += c == '\n';
    out += got;
  }
  return out;
}
