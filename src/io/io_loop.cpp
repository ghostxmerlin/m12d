#include "io/io_loop.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace {
const char kBusyLine[] = "{\"id\":-1,\"ok\":false,\"err\":\"busy\"}\n";

// Never raises SIGPIPE on sockets; plain write() for the tty.
ssize_t out_write(int fd, bool is_serial, const char* p, size_t n) {
  return is_serial ? ::write(fd, p, n) : ::send(fd, p, n, MSG_NOSIGNAL | MSG_DONTWAIT);
}
}  // namespace

IoLoop::IoLoop(int serial_fd, int listen_fd, size_t max_clients, Handler h)
    : listen_fd_(listen_fd), max_clients_(max_clients), handler_(std::move(h)) {
  wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (serial_fd >= 0) chans_.push_back(Chan{0, serial_fd, true});
}

IoLoop::~IoLoop() {
  for (Chan& ch : chans_)
    if (ch.fd >= 0) ::close(ch.fd);
  if (listen_fd_ >= 0) ::close(listen_fd_);
  if (wake_fd_ >= 0) ::close(wake_fd_);
}

IoLoop::Chan* IoLoop::find(ChanId c) {
  for (Chan& ch : chans_)
    if (ch.id == c && !ch.dead) return &ch;
  return nullptr;
}
const IoLoop::Chan* IoLoop::find(ChanId c) const {
  for (const Chan& ch : chans_)
    if (ch.id == c && !ch.dead) return &ch;
  return nullptr;
}

void IoLoop::set_subscribed(ChanId c, bool on) {
  if (Chan* ch = find(c)) ch->subscribed = on;
}
bool IoLoop::subscribed(ChanId c) const {
  const Chan* ch = find(c);
  return ch && ch->subscribed;
}

void IoLoop::push(std::string line) {
  queue_.push(std::move(line));
  const uint64_t one = 1;
  ssize_t r = ::write(wake_fd_, &one, sizeof one);
  (void)r;
}

void IoLoop::enqueue(Chan& ch, const std::string& line, bool is_reply) {
  if (ch.dead) return;
  // Whole lines only: if the line (plus '\n') does not fit, drop it. Replies get kReplyHeadroom
  // beyond the push cap, so a channel flooded with det lines still gets its replies.
  if (ch.out.size() + line.size() + 1 > kChanBufBytes + (is_reply ? kReplyHeadroom : 0)) {
    stats_.dropped++;
    return;
  }
  ch.out += line;
  ch.out += '\n';
}

void IoLoop::flush(Chan& ch) {
  while (!ch.dead && !ch.out.empty()) {
    ssize_t n = out_write(ch.fd, ch.is_serial, ch.out.data(), ch.out.size());
    if (n > 0) {
      ch.out.erase(0, static_cast<size_t>(n));
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
    close_chan(ch);
  }
}

void IoLoop::close_chan(Chan& ch) {
  if (ch.dead) return;
  if (ch.is_serial)
    std::fprintf(stderr, "io: serial channel error: %s; serial disabled\n", std::strerror(errno));
  else
    stats_.clients--;
  ::close(ch.fd);
  ch.fd = -1;
  ch.dead = true;
  ch.out.clear();
}

void IoLoop::read_input(Chan& ch) {
  // Bounded work per poll round (kReadBudget bytes): a client that floods input cannot
  // starve the other channels; poll() is level-triggered and will call us again.
  char buf[1024];
  for (size_t budget = kReadBudget; budget >= sizeof buf; budget -= sizeof buf) {
    ssize_t n = ::read(ch.fd, buf, sizeof buf);
    if (n > 0) {
      ch.splitter.feed(buf, static_cast<size_t>(n), [&](std::string_view line, bool too_long) {
        std::string reply = handler_(ch.id, line, too_long);
        if (!reply.empty()) enqueue(ch, reply, true);
      });
      if (ch.dead) return;
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
    close_chan(ch);  // EOF or error
    return;
  }
}

void IoLoop::accept_clients() {
  for (;;) {
    int fd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0) return;
    if (stats_.clients >= max_clients_) {
      ssize_t r = ::send(fd, kBusyLine, sizeof kBusyLine - 1, MSG_NOSIGNAL | MSG_DONTWAIT);
      (void)r;
      ::close(fd);
      stats_.rejected_clients++;
      continue;
    }
    int sz = kClientSndBuf, one = 1;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof sz);
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    chans_.push_back(Chan{next_id_++, fd, false});
    stats_.clients++;
  }
}

void IoLoop::reply(ChanId c, std::string line) {
  {
    std::lock_guard<std::mutex> lk(reply_mu_);
    replies_.emplace_back(c, std::move(line));
  }
  const uint64_t one = 1;
  ssize_t r = ::write(wake_fd_, &one, sizeof one);
  (void)r;
}

void IoLoop::drain_push_queue() {
  uint64_t v;
  while (::read(wake_fd_, &v, sizeof v) > 0) {
  }
  std::vector<std::pair<ChanId, std::string>> rs;
  {
    std::lock_guard<std::mutex> lk(reply_mu_);
    rs.swap(replies_);
  }
  for (auto& [c, line] : rs)
    if (Chan* ch = find(c))
      if (!ch->dead) enqueue(*ch, line, true);
  std::string line;
  while (queue_.pop(line, 0))
    for (Chan& ch : chans_)
      if (ch.subscribed) enqueue(ch, line);
  const uint64_t qd = queue_.dropped();
  if (qd > queue_drops_seen_) {
    stats_.dropped += qd - queue_drops_seen_;
    queue_drops_seen_ = qd;
  }
}

void IoLoop::run(std::atomic<bool>& stop) {
  std::vector<pollfd> pfds;
  std::vector<ChanId> ids;
  while (!stop) {
    pfds.clear();
    ids.clear();
    pfds.push_back({wake_fd_, POLLIN, 0});
    ids.push_back(-1);
    if (listen_fd_ >= 0) {
      pfds.push_back({listen_fd_, POLLIN, 0});
      ids.push_back(-2);
    }
    for (const Chan& ch : chans_) {
      pfds.push_back({ch.fd, static_cast<short>(POLLIN | (ch.out.empty() ? 0 : POLLOUT)), 0});
      ids.push_back(ch.id);
    }
    int r = ::poll(pfds.data(), pfds.size(), 200);
    if (r < 0 && errno != EINTR) {
      std::fprintf(stderr, "io: poll: %s\n", std::strerror(errno));
      break;
    }
    if (r <= 0) continue;
    for (size_t i = 0; i < pfds.size(); i++) {
      const short ev = pfds[i].revents;
      if (!ev) continue;
      if (ids[i] == -1) {
        drain_push_queue();
      } else if (ids[i] == -2) {
        accept_clients();
      } else if (Chan* ch = find(ids[i])) {
        if (ev & (POLLIN | POLLHUP | POLLERR)) read_input(*ch);
      }
    }
    for (Chan& ch : chans_) flush(ch);
    chans_.erase(std::remove_if(chans_.begin(), chans_.end(), [](const Chan& c) { return c.dead; }), chans_.end());
  }
}
