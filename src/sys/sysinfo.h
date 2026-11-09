#pragma once
#include <cstdint>
#include <string>

long parse_mem_available_kb(const std::string& meminfo);  // -1 if absent
int64_t parse_uptime_s(const std::string& uptime);        // -1 if malformed
float parse_cpu_mhz(const std::string& scaling_cur_freq); // kHz text -> MHz; 0 if malformed

long mem_available_kb();  // /proc/meminfo MemAvailable
int64_t uptime_s();       // /proc/uptime
float cpu_cur_mhz();      // cpu0 scaling_cur_freq
