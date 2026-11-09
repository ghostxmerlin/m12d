#!/bin/sh
# Run a command inside the m12d build container with the repo at /work.
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
IMG=m12d-build
if ! docker image inspect "$IMG" >/dev/null 2>&1; then
  docker build --platform linux/arm64 -t "$IMG" "$REPO/docker"
fi
# ../research (model files for host tests) is mounted read-only at /research, so the
# repo-relative path ../research/... resolves the same inside the container.
RES=""
[ -d "$REPO/../research" ] && RES="-v $(cd "$REPO/../research" && pwd):/research:ro"
exec docker run --rm -v "$REPO":/work $RES -w /work "$IMG" "$@"
