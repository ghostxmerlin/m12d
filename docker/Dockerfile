FROM --platform=linux/arm64 ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      cmake ninja-build g++ make curl ca-certificates xz-utils file python3 git \
    && rm -rf /var/lib/apt/lists/*
# ARM official cross toolchain, aarch64 host -> arm-none-linux-gnueabihf.
# SHA-256 pinned; the file's MD5 matched ARM's published .asc on 2026-11-09.
RUN curl -fsSL -o /tmp/gcc.tar.xz \
      https://developer.arm.com/-/media/Files/downloads/gnu-a/10.2-2020.11/binrel/gcc-arm-10.2-2020.11-aarch64-arm-none-linux-gnueabihf.tar.xz \
    && echo 'd169f9196e3a6c4248ee79ca85987ebce0e4ea9174c1f8d51af9b28fecf22da1  /tmp/gcc.tar.xz' | sha256sum -c - \
    && mkdir -p /opt/gcc-arm \
    && tar -xJf /tmp/gcc.tar.xz -C /opt/gcc-arm --strip-components=1 \
    && rm /tmp/gcc.tar.xz
ENV PATH=/opt/gcc-arm/bin:$PATH
# Host-only OpenCV (core + imgproc) for the stock reference oracle in tools/ref.
# Never linked into device binaries.
RUN apt-get update && apt-get install -y --no-install-recommends \
      libopencv-core-dev libopencv-imgproc-dev \
    && rm -rf /var/lib/apt/lists/*
# protobuf for ncnn's onnx2ncnn (host-side model conversion only).
RUN apt-get update && apt-get install -y --no-install-recommends libprotobuf-dev protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*
