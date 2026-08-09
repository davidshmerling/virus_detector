#!/bin/bash
# Restoring a single quarantined entry by id must put the file back and remove
# it from the quarantine list.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir restore_one)"
seed_file "$DIR/bad1.txt" "$SIG_A"

av scan "$DIR" >/dev/null

ID="$(av q-list | awk '/ID:/ {print $2; exit}')"
[[ -n "$ID" ]] || fail "could not read a quarantine id"

av restore "$ID"   # prints nothing; exit code is the contract

[[ -f "$DIR/bad1.txt" ]] || fail "file was not restored to its original path"
if av q-list | grep -q "$ID"; then
    fail "id $ID still listed after restore"
fi
