#!/bin/sh
# Run a command inside the m12d build container with the repo at /work.
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
IMG=m12d-build
if ! docker image inspect "$IMG" >/dev/null 2>&1; then
  docker build --platform linux/arm64 -t "$IMG" "$REPO/docker"
fi
exec docker run --rm -v "$REPO":/work -w /work "$IMG" "$@"
