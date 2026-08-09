#!/bin/bash
# Files containing real signatures must be detected and moved into quarantine.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir mal_quarantine)"
seed_file "$DIR/bad1.txt" "$SIG_A"
seed_file "$DIR/bad2.txt" "$SIG_B"

av scan "$DIR" >/dev/null

OUT="$(av q-list)"
echo "$OUT"

grep -q "bad1.txt" <<< "$OUT" || fail "bad1.txt was not quarantined"
grep -q "bad2.txt" <<< "$OUT" || fail "bad2.txt was not quarantined"

# Quarantine MOVES the file out of its original location.
[[ ! -e "$DIR/bad1.txt" ]] || fail "bad1.txt still present after quarantine"
[[ ! -e "$DIR/bad2.txt" ]] || fail "bad2.txt still present after quarantine"
