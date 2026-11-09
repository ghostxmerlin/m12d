#pragma once
#include <cstdint>

// Listens on 0.0.0.0:port (0 = ephemeral), SO_REUSEADDR, non-blocking. Returns fd or -1.
int open_tcp_listener(uint16_t port, int backlog);
