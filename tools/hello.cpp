#include <cstdio>
#include <thread>

int main() {
  bool ok = false;
  std::thread t([&] { ok = true; });
  t.join();
  std::printf(ok ? "hello m12 threads ok\n" : "hello m12 threads FAILED\n");
  return ok ? 0 : 1;
}
