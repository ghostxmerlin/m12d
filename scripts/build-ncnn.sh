#!/bin/sh
# Build static ncnn for armhf (device) and host (container) into third_party/ncnn-install/<arch>.
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
SRC="$REPO/third_party/ncnn"
COMMON="-DCMAKE_BUILD_TYPE=Release -DNCNN_VULKAN=OFF -DNCNN_OPENMP=ON -DNCNN_SHARED_LIB=OFF \
  -DNCNN_BUILD_TOOLS=OFF -DNCNN_BUILD_EXAMPLES=OFF -DNCNN_BUILD_BENCHMARK=OFF -DNCNN_BUILD_TESTS=OFF"
build() {  # $1=arch  $2..=extra cmake args
  arch=$1; shift
  if [ -f "$REPO/third_party/ncnn-install/$arch/lib/libncnn.a" ]; then
    echo "build-ncnn: $arch already built"; return
  fi
  cmake -S "$SRC" -B "$REPO/build/ncnn-$arch" -G Ninja $COMMON \
    -DCMAKE_INSTALL_PREFIX="$REPO/third_party/ncnn-install/$arch" "$@"
  ninja -C "$REPO/build/ncnn-$arch" install
}
build armhf -DCMAKE_TOOLCHAIN_FILE="$REPO/cmake/toolchain-armhf.cmake" -DNCNN_ARM82=OFF
build host -DNCNN_ARM82=OFF
file "$REPO/third_party/ncnn-install/armhf/lib/libncnn.a" "$REPO/third_party/ncnn-install/host/lib/libncnn.a"
