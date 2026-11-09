#!/bin/sh
# scripts/measure.sh <name> <seconds> -- '<device cmd>'
# Runs <device cmd> as root in /home/m5stack/m12d (the cmd may cd elsewhere), samples every
# 10 s in ONE ssh session: temperature, total CPU busy %, target alive, target RSS.
# Aborts (kills the target) at >= 90 C. At the end sends SIGTERM and records the exit code
# and the average fps (stock: 1000/avg of stderr "total" lines; m12d: stderr avg_fps=).
# Writes results/<name>.csv and prints a summary line.
set -e
[ "$3" = "--" ] && [ -n "$4" ] || { echo "usage: measure.sh <name> <seconds> -- '<device cmd>'" >&2; exit 1; }
NAME=$1; SECS=$2; CMD=$4
DIR=$(cd "$(dirname "$0")/.." && pwd)
CSV="$DIR/results/$NAME.csv"
B64=$(printf '%s' "$CMD" | base64 | tr -d '\n')
"$DIR/scripts/m12ssh.sh" --root "
SECS=$SECS
ERR=/tmp/measure_$NAME.err
CMD=\$(echo $B64 | base64 -d)
cd /home/m5stack/m12d
sh -c \"\$CMD\" > /dev/null 2> \$ERR < /dev/null &
JOB=\$!
sleep 2
PID=\$(ps | awk '/\\.\\/bin\\/(m12d|face_detector)/ && !/awk/ {print \$1; exit}')
cpu() { awk '/^cpu /{print \$2+\$3+\$4+\$7+\$8+\$9, \$5+\$6}' /proc/stat; }
set -- \$(cpu); PB=\$1; PI=\$2
echo 'time_s,temp_c,cpu_pct,alive,rss_kb'
t=0; status=OK
while [ \$t -lt \$SECS ]; do
  sleep 10; t=\$((t+10))
  temp=\$(awk '{print \$2}' /sys/class/mstar/msys/TEMP_R)
  set -- \$(cpu); DB=\$((\$1-PB)); DI=\$((\$2-PI)); PB=\$1; PI=\$2
  pct=\$(( DB*100 / (DB+DI) ))
  if [ -n \"\$PID\" ] && [ -d /proc/\$PID ]; then alive=1; rss=\$(awk '/VmRSS/{print \$2}' /proc/\$PID/status); else alive=0; rss=0; fi
  echo \"\$t,\$temp,\$pct,\$alive,\$rss\"
  if [ \$temp -ge 90 ]; then status=ABORT; break; fi
  if [ \$alive -eq 0 ]; then status=DIED; break; fi
done
[ -n \"\$PID\" ] && kill -TERM \$PID 2>/dev/null
wait \$JOB; rc=\$?
for p in \$(ps | awk '/sleep 999999/ && !/awk/ {print \$1}'); do kill \$p; done
if grep -q '^total ' \$ERR; then
  fps=\$(awk '/^total /{s+=\$2;n++} END{if(n) printf \"%.2f\", 1000/(s/n)}' \$ERR)
else
  fps=\$(awk -F'avg_fps=' '/avg_fps=/{print \$2}' \$ERR)
fi
echo \"#end status=\$status rc=\$rc fps=\$fps pid=\$PID\"
" | tee "$CSV"
/usr/bin/python3 - "$CSV" "$SECS" <<'PY'
import sys
path, secs = sys.argv[1], int(sys.argv[2])
rows, end = [], ""
for line in open(path):
    line = line.strip()
    if line.startswith("#end"):
        end = line[5:]
    elif line and line[0].isdigit():
        rows.append([int(x) for x in line.split(",")])
if not rows:
    sys.exit("measure: no samples")
temps = [r[1] for r in rows]
last = [r[1] for r in rows if r[0] > secs - 60] or temps[-1:]
cpu = [r[2] for r in rows]
print(f"SUMMARY {path.split('/')[-1][:-4]}: avg_temp={sum(temps)/len(temps):.1f} max_temp={max(temps)} "
      f"last_min_temp={sum(last)/len(last):.1f} avg_cpu={sum(cpu)/len(cpu):.1f} "
      f"alive={'yes' if all(r[3] for r in rows) else 'no'} rss_first={rows[0][4]} rss_last={rows[-1][4]} "
      f"rss_delta={rows[-1][4]-rows[0][4]} {end}")
PY
