#!/bin/bash
# restore-all must bring every quarantined file back and empty the quarantine.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir restore_all)"
seed_file "$DIR/bad1.txt" "$SIG_A"
seed_file "$DIR/bad2.txt" "$SIG_B"

av scan "$DIR" >/dev/null

av restore-all

[[ -f "$DIR/bad1.txt" ]] || fail "bad1.txt not restored by restore-all"
[[ -f "$DIR/bad2.txt" ]] || fail "bad2.txt not restored by restore-all"

OUT="$(av q-list)"
echo "$OUT"
grep -q "Quarantine is empty" <<< "$OUT" || fail "quarantine not empty after restore-all"
