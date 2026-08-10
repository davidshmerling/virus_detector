#!/bin/bash
# A corrupt cache.db must not crash the scanner. load() fails open / returns an
# empty map; the scan continues and still prints a summary.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir corrupt_cache)"
seed_file "$DIR/a.txt" "harmless"

mkdir -p "$RUNDIR/runtime/cache"
printf 'this is not a sqlite database\n' > "$RUNDIR/runtime/cache/cache.db"

set +e
OUT="$(av scan "$DIR" 2>&1)"
RC=$?
set -e

echo "$OUT"
[[ "$RC" -eq 0 ]] || fail "scan exited $RC on corrupt cache (expected 0)"
grep -q "Scan summary:" <<< "$OUT" || fail "scan produced no summary with corrupt cache"
malicious="$(summary_field "$OUT" Malicious)"
[[ "${malicious:-x}" == "0" ]] || fail "unexpected malicious count '$malicious'"
