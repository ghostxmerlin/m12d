#!/bin/sh
# Push a file to the M12 (no scp/sftp there): scripts/m12push.sh <local> <remote>
set -e
[ -f "$1" ] || { echo "m12push: missing $1" >&2; exit 1; }
DIR=$(cd "$(dirname "$0")" && pwd)
[ -n "$M12_PASS" ] || { echo "m12push: M12_PASS is not set" >&2; exit 2; }
HOST=${M12_HOST:-10.254.239.1}
USER_=${M12_USER:-m5stack}
T=$(mktemp -d)
# Device host key is pinned on first use (git-ignored); a changed key is refused.
KH=$(cd "$(dirname "$0")/.." && pwd)/.superpowers/m12_known_hosts
mkdir -p "$(dirname "$KH")"
trap 'rm -rf "$T"' EXIT INT TERM
printf '#!/bin/sh\necho "$M12_PASS"\n' > "$T/askpass"
chmod 700 "$T/askpass"
SSH_ASKPASS="$T/askpass" SSH_ASKPASS_REQUIRE=force DISPLAY=:0 \
  ssh -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile="$KH" -o LogLevel=ERROR \
      -o PubkeyAuthentication=no -o ConnectTimeout=8 \
      "$USER_@$HOST" "cat > '$2'" < "$1"
