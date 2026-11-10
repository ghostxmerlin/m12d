#pragma once
#include <cstddef>
#include <string>

// Writes data to path with mode 0600: written to a fresh mkstemp() file in the same
// directory, then rename()d over path. A symlink at path is replaced, never followed.
// durable: also fsync the file before the rename and the directory after it (power-cut safe).
bool write_file_private(const std::string& path, const void* data, size_t n, bool durable = false);
