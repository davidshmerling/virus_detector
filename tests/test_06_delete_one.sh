#!/bin/bash
# Deleting a quarantined entry must remove it permanently: gone from the list
# and NOT restored to its original path.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir delete_one)"
seed_file "$DIR/bad1.txt" "$SIG_A"

av scan "$DIR" >/dev/null

ID="$(av q-list | awk '/ID:/ {print $2; exit}')"
[[ -n "$ID" ]] || fail "could not read a quarantine id"

av delete "$ID"   # prints nothing; exit code is the contract

if av q-list | grep -q "$ID"; then
    fail "id $ID still listed after delete"
fi
[[ ! -e "$DIR/bad1.txt" ]] || fail "deleted file reappeared at its original path"
