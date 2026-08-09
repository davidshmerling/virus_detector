#!/bin/bash
# A path listed in config/exclude.txt must be skipped entirely, even if it
# contains a signature. The sandbox's exclude.txt already lists $DATA/exclude
# (written by init_sandbox), so here we just drop a malicious file in it and
# confirm the scan skips the whole directory.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir exclude)"        # this is exactly $DATA/exclude
seed_file "$DIR/should_not_scan.txt" "$SIG_A"

OUT="$(av scan "$DIR")"
echo "$OUT"

scanned="$(summary_field "$OUT" Scanned)"
quarantined="$(summary_field "$OUT" Quarantined)"

[[ "${scanned:-x}" == "0" ]]     || fail "excluded root was scanned (scanned='$scanned')"
[[ "${quarantined:-x}" == "0" ]] || fail "excluded file was quarantined (quarantined='$quarantined')"
[[ -f "$DIR/should_not_scan.txt" ]] || fail "excluded malicious file was moved away"
