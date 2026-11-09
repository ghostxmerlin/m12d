#!/bin/sh
# Fetch pinned third-party sources into third_party/. Idempotent.
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
TP="$REPO/third_party"
NCNN_COMMIT=6afc736225de2f1b193292e31ca6242d138463d4
mkdir -p "$TP"
if [ ! -f "$TP/ncnn/.fetched-$NCNN_COMMIT" ]; then
  rm -rf "$TP/ncnn"
  git init -q "$TP/ncnn"
  git -C "$TP/ncnn" remote add origin https://github.com/Tencent/ncnn.git
  git -C "$TP/ncnn" fetch -q --depth 1 origin "$NCNN_COMMIT"
  git -C "$TP/ncnn" checkout -q FETCH_HEAD
  touch "$TP/ncnn/.fetched-$NCNN_COMMIT"
fi
if [ ! -f "$TP/doctest/doctest.h" ]; then
  mkdir -p "$TP/doctest"
  curl -fsSL -o "$TP/doctest/doctest.h" \
    https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
fi
echo "fetch-deps: ncnn@$NCNN_COMMIT doctest@2.4.11 ready"
