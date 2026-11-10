#!/bin/sh
# m12d autostart on the M12: scripts/boot.sh install|remove|status
#   install: back up S85runpayload (once, to the SD card), patch it to skip the stock payload
#            when /media/sdcard/m12-m12d exists, install /etc/init.d/S99m12d, create the flag.
#   remove:  restore S85runpayload from the backup, delete S99m12d and the flag (stock on reboot).
#   status:  show flag, patch state, S99m12d, running m12d.
# The stock thermal guard (S110thermal) is never touched. Takes effect on the next boot.
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
case "$1" in
  install)
    "$DIR/m12push.sh" "$DIR/../device/init.d/S99m12d" /tmp/S99m12d
    "$DIR/m12push.sh" "$DIR/../device/init.d/S85-skip.inc" /tmp/S85-skip.inc
    "$DIR/m12ssh.sh" --root '
set -e
B=/media/sdcard/m12test/S85runpayload.orig
grep -q " /media/sdcard " /proc/mounts || { echo "SD card not mounted"; exit 3; }
mkdir -p /media/sdcard/m12test
[ -f $B ] || cp /etc/init.d/S85runpayload $B
if ! grep -q m12-m12d /etc/init.d/S85runpayload; then
  sed "/^[[:space:]]*start)/r /tmp/S85-skip.inc" $B > /tmp/S85.new
  grep -q m12-m12d /tmp/S85.new || { echo "patch failed"; exit 4; }
  cp /tmp/S85.new /etc/init.d/S85runpayload
fi
rm -f /tmp/S85.new /tmp/S85-skip.inc
cp /tmp/S99m12d /etc/init.d/S99m12d && rm -f /tmp/S99m12d
chmod 755 /etc/init.d/S85runpayload /etc/init.d/S99m12d
touch /media/sdcard/m12-m12d
sync
echo "installed: next boot starts m12d"'
    ;;
  remove)
    "$DIR/m12ssh.sh" --root '
B=/media/sdcard/m12test/S85runpayload.orig
[ -f $B ] && cp $B /etc/init.d/S85runpayload && chmod 755 /etc/init.d/S85runpayload
rm -f /etc/init.d/S99m12d /media/sdcard/m12-m12d
sync
echo "removed: next boot is stock"'
    ;;
  status)
    "$DIR/m12ssh.sh" '
[ -f /media/sdcard/m12-m12d ] && echo "flag: present" || echo "flag: absent"
grep -q m12-m12d /etc/init.d/S85runpayload && echo "S85runpayload: patched" || echo "S85runpayload: stock"
[ -f /etc/init.d/S99m12d ] && echo "S99m12d: installed" || echo "S99m12d: absent"
ps | awk "/bin\/m12d|server_core|check_thermal/ && !/awk/ {print \"proc:\", \$0}"'
    ;;
  *) echo "usage: boot.sh install|remove|status" >&2; exit 1 ;;
esac
