#!/bin/sh
# Build device binaries, verify they are static ARM, push them to the M12 and make sure
# the model is in /home/m5stack/m12d/models (copied from the stock payload, read-only use).
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR/.."
BINS="m12d m12bench m12cap detect_file"
scripts/env.sh sh -c "scripts/build.sh target >/dev/null && for b in $BINS; do scripts/check-binary.sh build/target/\$b; done"
scripts/m12ssh.sh 'mkdir -p /home/m5stack/m12d/bin /home/m5stack/m12d/models /home/m5stack/m12d/log'
for b in $BINS; do
  scripts/m12push.sh "build/target/$b" "/home/m5stack/m12d/bin/$b.new"
done
# genderage (insightface buffalo_s, converted to ncnn) lives in ../research, not on the device.
for f in genderage.param genderage.bin; do
  scripts/m12push.sh "../research/2026-11-09-genderage/$f" "/home/m5stack/m12d/models/$f.new"
done
scripts/m12ssh.sh "cd /home/m5stack/m12d/models && for f in genderage.param genderage.bin; do mv -f \$f.new \$f; done"
scripts/m12ssh.sh "cd /home/m5stack/m12d/bin && for b in $BINS; do chmod +x \$b.new && mv -f \$b.new \$b; done
cd /home/m5stack/m12d/models && for f in mnet.25-opt.param mnet.25-opt.bin; do [ -f \$f ] || cp /home/m5stack/payload/models/\$f .; done
ls -l /home/m5stack/m12d/bin /home/m5stack/m12d/models"
echo "deploy: OK"
