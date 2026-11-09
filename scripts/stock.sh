#!/bin/sh
# Stock services on the M12: scripts/stock.sh status|stop
# stop kills server.py, server_core.py and their ./bin/* child by PID only.
# Never killall python3 (that also kills check_thermal.py). Restore stock by rebooting.
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
case "$1" in
  status)
    "$DIR/m12ssh.sh" 'ps | awk "/server\.py|server_core\.py|\.\/bin\/|check_thermal/ && !/awk/"'
    ;;
  stop)
    "$DIR/m12ssh.sh" --root '
pids=$(ps | awk "/server\.py|server_core\.py|\.\/bin\// && !/awk/ && !/m12d/ {print \$1}")
for p in $pids; do kill -9 $p && echo "killed $p"; done
sleep 1
left=$(ps | awk "/server\.py|server_core\.py/ && !/awk/" | wc -l)
therm=$(ps | awk "/check_thermal/ && !/awk/" | wc -l)
echo "stock_left=$left thermal_running=$therm"
[ "$therm" -ge 1 ] || { echo "WARNING: check_thermal.py is NOT running"; exit 3; }
[ "$left" -eq 0 ]
'
    ;;
  *) echo "usage: stock.sh status|stop" >&2; exit 1 ;;
esac
