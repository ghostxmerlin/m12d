#!/bin/sh
# Phase-2 A-layer: stock arcface feature path vs FaceEmbedder on the same 320x240 RGB and the same
# (truncated) landmarks, over tests/data/faceid/*.yuyv. Run inside scripts/env.sh after build.sh host.
# Prints one cosine per frame and the minimum; exit 1 if any is below 0.999.
M=../research/2026-11-09-app-readiness/official-recovery-files/home/m5stack/payload/models
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
min=1; n=0
for f in tests/data/faceid/*.yuyv; do
  build/host/embed_file $M/mnet.25-opt $M/mobilefacenet "$f" --dump-rgb "$T/x.rgb" --int-lm > "$T/ours" || { echo "SKIP $f (no face)"; continue; }
  lm=$(head -1 "$T/ours" | cut -d' ' -f2-)
  tail -n +2 "$T/ours" > "$T/a"
  build/host/ref_stock_arc $M "$T/x.rgb" $lm > "$T/b" || exit 2
  c=$(paste "$T/a" "$T/b" | awk '{s+=$1*$2} END {printf "%.6f", s}')
  echo "$(basename $f) cos=$c"
  min=$(echo "$min $c" | awk '{print ($2<$1)?$2:$1}')
  n=$((n+1))
done
echo "frames=$n min_cos=$min"
awk -v m="$min" 'BEGIN {exit !(m >= 0.999)}'
