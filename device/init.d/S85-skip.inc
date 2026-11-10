                # m12d autostart (scripts/boot.sh): skip the stock payload when enabled
                i=0
                while [ $i -lt 8 ] && ! grep -q " /media/sdcard " /proc/mounts; do sleep 1; i=$((i+1)); done
                if [ -f /media/sdcard/m12-m12d ]; then echo "payload skipped (m12d enabled: /media/sdcard/m12-m12d)"; exit 0; fi
