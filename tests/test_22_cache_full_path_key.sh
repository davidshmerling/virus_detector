#!/bin/bash
# Cache is keyed by full path. Two files with the same basename in different
# directories must keep independent verdicts (one clean, one malicious).
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir same_basename)"
mkdir -p "$DIR/left" "$DIR/right"
seed_file "$DIR/left/foo.txt" "completely clean"
seed_file "$DIR/right/foo.txt" "$SIG_A"

OUT1="$(av scan "$DIR")"
echo "--- first ---"
echo "$OUT1"

[[ ! -e "$DIR/right/foo.txt" ]] || fail "malicious right/foo.txt was not quarantined"
[[ -f "$DIR/left/foo.txt" ]] || fail "clean left/foo.txt was wrongly quarantined"

malicious="$(summary_field "$OUT1" Malicious)"
[[ "${malicious:-x}" == "1" ]] || fail "expected exactly 1 malicious, got '$malicious'"

OUT2="$(av scan "$DIR")"
echo "--- second ---"
echo "$OUT2"

cached="$(summary_field "$OUT2" Cached)"
scanned="$(summary_field "$OUT2" Scanned)"
malicious2="$(summary_field "$OUT2" Malicious)"

[[ "${cached:-0}" -ge 1 ]] || fail "expected cache hit for left/foo.txt, cached='$cached'"
[[ "${scanned:-x}" == "0" ]] || fail "second scan should not re-scan, scanned='$scanned'"
[[ "${malicious2:-x}" == "0" ]] || fail "cached clean path must not flip to malicious"
[[ -f "$DIR/left/foo.txt" ]] || fail "left/foo.txt disappeared after second scan"
