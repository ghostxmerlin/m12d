#!/bin/sh
# Verify an ELF is a static ARM EABI5 hard-float VFPv4 binary.
f="$1"
[ -f "$f" ] || { echo "check-binary: missing $f"; exit 1; }
fail=0
info=$(file -b "$f")
echo "$info" | grep -q "ARM, EABI5" || { echo "check-binary: not ARM EABI5: $info"; fail=1; }
echo "$info" | grep -q "statically linked" || { echo "check-binary: not statically linked: $info"; fail=1; }
if readelf -d "$f" 2>&1 | grep -q NEEDED; then echo "check-binary: has NEEDED entries"; fail=1; fi
attrs=$(readelf -A "$f")
echo "$attrs" | grep -q "Tag_FP_arch: VFPv4" || { echo "check-binary: Tag_FP_arch is not VFPv4"; fail=1; }
echo "$attrs" | grep -q "Tag_ABI_VFP_args: VFP registers" || { echo "check-binary: not hard-float ABI"; fail=1; }
[ $fail -eq 0 ] && echo "check-binary: OK $f"
exit $fail
