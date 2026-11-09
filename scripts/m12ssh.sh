#!/bin/sh
# Run a command on the M12: scripts/m12ssh.sh [--root] '<cmd>'
# Password comes only from $M12_PASS; host from $M12_HOST (default 10.254.239.1).
set -e
ROOT=0
if [ "$1" = "--root" ]; then ROOT=1; shift; fi
[ -n "$M12_PASS" ] || { echo "m12ssh: M12_PASS is not set" >&2; exit 2; }
HOST=${M12_HOST:-10.254.239.1}
USER_=${M12_USER:-m5stack}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT INT TERM
printf '#!/bin/sh\necho "$M12_PASS"\n' > "$T/askpass"
chmod 700 "$T/askpass"
B64=$(printf '%s\n' "$1" | base64 | tr -d '\n')
if [ $ROOT -eq 1 ]; then
  # Script goes to a temp file; sudo reads the password from ssh stdin, so it never
  # appears on a command line.
  REMOTE="f=/tmp/.m12cmd.\$\$; echo $B64 | base64 -d > \$f; sudo -S -p '' sh \$f; r=\$?; rm -f \$f; exit \$r"
else
  REMOTE="echo $B64 | base64 -d | sh"
fi
RB64=$(printf '%s' "$REMOTE" | base64 | tr -d '\n')
SSH_ASKPASS="$T/askpass" SSH_ASKPASS_REQUIRE=force DISPLAY=:0 M12_PASS="$M12_PASS" \
  ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile="$T/kh" -o LogLevel=ERROR \
      -o PubkeyAuthentication=no -o ConnectTimeout=8 \
      "$USER_@$HOST" "sh -c \"\$(echo $RB64 | base64 -d)\"" <<IN
$( [ $ROOT -eq 1 ] && printf '%s' "$M12_PASS" )
IN
