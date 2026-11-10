#!/bin/sh
# Capture a labelled frame burst on the M12 and pull it to the Mac:
#   scripts/capset.sh <dir> <name> [n_saved=4] [every=10] [size=640x480]
# Captures at 15 fps, keeps every k-th frame after a ~20-frame auto-white-balance warm-up, writes
# tests/data/<dir>/<name>_NN.yuyv plus a preview PNG next to it (git-ignored: faces).
# Stock services must be stopped first (scripts/stock.sh stop): the camera is exclusive.
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
[ $# -ge 2 ] || { echo "usage: capset.sh <dir> <name> [n_saved] [every] [size]" >&2; exit 1; }
SUB=$1; NAME=$2; N=${3:-4}; EVERY=${4:-10}; SIZE=${5:-640x480}
case "$SUB$NAME" in *[!A-Za-z0-9_-]*) echo "capset: dir/name must be [A-Za-z0-9_-]" >&2; exit 1 ;; esac
OUT="$DIR/../tests/data/$SUB"
mkdir -p "$OUT"
DROP=$(( (20 + EVERY - 1) / EVERY ))   # saved frames inside the AWB warm-up
TOTAL=$(( (N + DROP) * EVERY ))
"$DIR/m12ssh.sh" --root "
rm -f /tmp/cap_${NAME}_*.yuyv
cd /home/m5stack/m12d && ./bin/m12cap $TOTAL /tmp/cap_$NAME 15 --every $EVERY --size $SIZE >&2
cd /tmp && tar -cf - cap_${NAME}_*.yuyv
rm -f /tmp/cap_${NAME}_*.yuyv
" | (cd "$OUT" && tar -xf -)
i=0
for f in "$OUT"/cap_"$NAME"_*.yuyv; do
  if [ $i -lt $DROP ]; then rm -f "$f"; i=$((i + 1)); continue; fi
  g=$(printf '%s/%s_%02d.yuyv' "$OUT" "$NAME" $((i - DROP)))
  i=$((i + 1))
  mv "$f" "$g"
  /usr/bin/python3 "$DIR/draw_boxes.py" "$g" "${g%.yuyv}.png" /dev/null
done
ls "$OUT"/"$NAME"_*.yuyv | wc -l | awk -v n="$NAME" '{print "capset: " n " -> " $1 " frames"}'
