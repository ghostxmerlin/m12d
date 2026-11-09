#include "io/tcp_listener.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

int open_tcp_listener(const std::string& bind_addr, uint16_t port, int backlog) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  if (inet_pton(AF_INET, bind_addr.c_str(), &a.sin_addr) != 1) {
    std::fprintf(stderr, "tcp: bad bind address %s\n", bind_addr.c_str());
    return -1;
  }
  int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) return -1;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  if (bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || listen(fd, backlog) != 0) {
    std::fprintf(stderr, "tcp: listen on %s:%u: %s\n", bind_addr.c_str(), port, std::strerror(errno));
    ::close(fd);
    return -1;
  }
  return fd;
}
