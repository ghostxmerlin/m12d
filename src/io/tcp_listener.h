#pragma once
#include <cstdint>
#include <string>

// Listens on bind_addr:port (IPv4 dotted quad; port 0 = ephemeral), SO_REUSEADDR,
// non-blocking. Returns fd or -1 (bad address, address not local, port in use).
int open_tcp_listener(const std::string& bind_addr, uint16_t port, int backlog);
