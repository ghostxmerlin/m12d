# Reference-only image: runs the original MXNet age/gender model to check the ncnn conversion.
# x86_64 on purpose: the aarch64 mxnet wheel needs ARM Performance Libraries (libarmpl).
FROM --platform=linux/amd64 ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends python3 python3-pip libgomp1 libquadmath0 libgfortran5 \
    && rm -rf /var/lib/apt/lists/*
RUN pip3 install --no-cache-dir "numpy<1.24" mxnet==1.9.1
