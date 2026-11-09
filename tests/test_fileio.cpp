#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#include "doctest.h"
#include "sys/fileio.h"

namespace {
std::string slurp(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
std::string tmpdir() {
  char t[] = "/tmp/m12fileioXXXXXX";
  return mkdtemp(t);
}
}  // namespace

TEST_CASE("write_file_private creates 0600 file with content") {
  const std::string d = tmpdir(), p = d + "/snap";
  REQUIRE(write_file_private(p, "abc", 3));
  CHECK(slurp(p) == "abc");
  struct stat st {};
  REQUIRE(lstat(p.c_str(), &st) == 0);
  CHECK((st.st_mode & 0777) == 0600);
  REQUIRE(write_file_private(p, "xy", 2));  // replaces
  CHECK(slurp(p) == "xy");
}

TEST_CASE("write_file_private replaces a symlink instead of following it") {
  const std::string d = tmpdir(), victim = d + "/victim", p = d + "/snap";
  { std::ofstream(victim) << "keep"; }
  REQUIRE(symlink(victim.c_str(), p.c_str()) == 0);
  REQUIRE(write_file_private(p, "new", 3));
  CHECK(slurp(victim) == "keep");
  struct stat st {};
  REQUIRE(lstat(p.c_str(), &st) == 0);
  CHECK(S_ISREG(st.st_mode));
  CHECK(slurp(p) == "new");
}

TEST_CASE("write_file_private fails in a missing directory") {
  CHECK_FALSE(write_file_private("/nonexistent-dir/x", "a", 1));
}
