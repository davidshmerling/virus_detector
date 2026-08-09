#!/bin/bash
# A large (multi-chunk) benign file must be scanned successfully: scanned,
# clean, and not counted as a failure.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir large)"
dd if=/dev/zero of="$DIR/large.bin" bs=1M count=50 status=none

OUT="$(av scan "$DIR")"
echo "$OUT"

scanned="$(summary_field "$OUT" Scanned)"
malicious="$(summary_field "$OUT" Malicious)"
failed="$(summary_field "$OUT" Failed)"

[[ "${scanned:-0}" -ge 1 ]]    || fail "large file was not scanned (scanned='$scanned')"
[[ "${malicious:-x}" == "0" ]] || fail "large zero-filled file flagged malicious ($malicious)"
[[ "${failed:-x}" == "0" ]]    || fail "large file failed to scan (failed='$failed')"
