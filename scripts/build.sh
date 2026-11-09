#!/bin/sh
# scripts/build.sh host|target — configure + build (+ ctest for host). Run inside scripts/env.sh.
set -e
cd "$(dirname "$0")/.."
case "$1" in
  host)
    cmake -S . -B build/host -G Ninja -DHOST=ON >/dev/null
    ninja -C build/host
    (cd build/host && ctest --output-on-failure)
    ;;
  target)
    cmake -S . -B build/target -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-armhf.cmake -DHOST=OFF >/dev/null
    ninja -C build/target
    ;;
  *) echo "usage: build.sh host|target" >&2; exit 1 ;;
esac
