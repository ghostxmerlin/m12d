#!/bin/sh
# B-layer parity over tests/data/set/*.yuyv (run inside scripts/env.sh after build.sh host).
# Writes per-frame results under build/parity/ and prints one PASS/FAIL line per frame per check.
M=../research/2026-11-09-app-readiness/official-recovery-files/home/m5stack/payload/models/mnet.25-opt
OUT=build/parity
mkdir -p $OUT
fail=0
for f in tests/data/set/*.yuyv; do
  n=$(basename $f .yuyv)
  build/host/detect_file $M $f --dump-rgb $OUT/$n.rgb > $OUT/$n.ours.txt
  build/host/ref_stock_fd $M --rgb $OUT/$n.rgb > $OUT/$n.ref_b1.txt
  build/host/ref_stock_fd $M $f > $OUT/$n.ref_b2.txt
  python3 scripts/parity.py $OUT/$n.ref_b1.txt $OUT/$n.ours.txt --coord 0.5 --prob 0.001 --label "B1 $n" || fail=1
  python3 scripts/parity.py $OUT/$n.ref_b2.txt $OUT/$n.ours.txt --coord 2 --prob 0.02 --label "B2 $n" || fail=1
done
exit $fail
