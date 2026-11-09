#pragma once
#include <cstddef>
#include <string>

// Writes data to path with mode 0600: written to a fresh mkstemp() file in the same
// directory, then rename()d over path. A symlink at path is replaced, never followed.
bool write_file_private(const std::string& path, const void* data, size_t n);
