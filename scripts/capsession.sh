#!/bin/sh
# Voice-guided capture session: scripts/capsession.sh <dir> <shotlist> [first_shot_name]
# Shot list lines: name|n_saved|every|wait_s|spoken prompt   (# comments and blank lines skipped)
# For each shot the Mac speaks the prompt, waits wait_s for the subject to get into place, says
# "开始", captures with capset.sh, then says "好". Aborts if the chip reaches 88C.
# With first_shot_name, earlier shots are skipped (resume after an abort).
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
[ $# -ge 2 ] || { echo "usage: capsession.sh <dir> <shotlist> [first_shot]" >&2; exit 1; }
SUB=$1; LIST=$2; FROM=${3:-}
VOICE=Tingting
speak() { say -v "$VOICE" "$1" || true; }
grep -v '^[[:space:]]*#' "$LIST" | grep -v '^[[:space:]]*$' > "${TMPDIR:-/tmp}/capsession.$$"
trap 'rm -f "${TMPDIR:-/tmp}/capsession.$$"' EXIT INT TERM
while IFS='|' read -r name n every wait prompt; do
  if [ -n "$FROM" ]; then [ "$name" = "$FROM" ] && FROM= || continue; fi
  temp=$("$DIR/m12ssh.sh" 'awk "{print \$2}" /sys/class/mstar/msys/TEMP_R' </dev/null)
  if [ "$temp" -ge 88 ]; then speak "设备太热，暂停"; echo "capsession: ABORT temp=$temp before $name" >&2; exit 3; fi
  echo "== $name (temp ${temp}C): $prompt"
  speak "$prompt"
  sleep "$wait"
  speak "开始"
  "$DIR/capset.sh" "$SUB" "$name" "$n" "$every" </dev/null 2>&1 | grep -v '^cap frames\|^saved=' || true
  speak "好"
done < "${TMPDIR:-/tmp}/capsession.$$"
speak "这一组拍完了"
echo "capsession: done"
