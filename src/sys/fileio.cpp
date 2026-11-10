#include "sys/fileio.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <vector>

bool write_file_private(const std::string& path, const void* data, size_t n, bool durable) {
  std::vector<char> tmpl(path.begin(), path.end());
  const std::string suffix = ".tmpXXXXXX";
  tmpl.insert(tmpl.end(), suffix.begin(), suffix.end());
  tmpl.push_back('\0');
  int fd = mkstemp(tmpl.data());  // O_EXCL, mode 0600
  if (fd < 0) return false;
  const char* p = static_cast<const char*>(data);
  size_t off = 0;
  while (off < n) {
    ssize_t w = ::write(fd, p + off, n - off);
    if (w <= 0) break;
    off += static_cast<size_t>(w);
  }
  const bool ok = off == n && fchmod(fd, 0600) == 0 && (!durable || ::fsync(fd) == 0) && ::close(fd) == 0;
  if (!ok || std::rename(tmpl.data(), path.c_str()) != 0) {
    if (!ok) ::close(fd);
    ::unlink(tmpl.data());
    return false;
  }
  if (durable) {
    const size_t slash = path.rfind('/');
    const std::string dir = slash == std::string::npos ? "." : slash == 0 ? "/" : path.substr(0, slash);
    const int dfd = ::open(dir.c_str(), O_RDONLY);
    if (dfd < 0) return false;
    const bool synced = ::fsync(dfd) == 0;
    ::close(dfd);
    return synced;
  }
  return true;
}
