#pragma once
#include <string>

// Opens a tty in raw 8N1 mode, non-blocking, no controlling terminal. Returns fd or -1.
// Supported baud: 9600, 19200, 38400, 57600, 115200.
int open_serial(const std::string& path, int baud);
