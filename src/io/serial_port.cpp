#include "io/serial_port.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

int open_serial(const std::string& path, int baud) {
  speed_t sp;
  switch (baud) {
    case 9600: sp = B9600; break;
    case 19200: sp = B19200; break;
    case 38400: sp = B38400; break;
    case 57600: sp = B57600; break;
    case 115200: sp = B115200; break;
    default: return -1;
  }
  int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    std::fprintf(stderr, "serial: open %s: %s\n", path.c_str(), std::strerror(errno));
    return -1;
  }
  termios t{};
  if (tcgetattr(fd, &t) != 0) {
    ::close(fd);
    return -1;
  }
  cfmakeraw(&t);
  t.c_cflag |= CLOCAL | CREAD;
  t.c_cflag &= ~(CSTOPB | PARENB | CRTSCTS);
  t.c_cflag = (t.c_cflag & ~CSIZE) | CS8;
  cfsetispeed(&t, sp);
  cfsetospeed(&t, sp);
  if (tcsetattr(fd, TCSANOW, &t) != 0) {
    ::close(fd);
    return -1;
  }
  tcflush(fd, TCIOFLUSH);
  return fd;
}
