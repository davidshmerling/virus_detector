#!/bin/bash
# Regression test for the quarantine/restore permission bug: a quarantined file
# must be restored with its ORIGINAL permissions, not a mangled/default mode.
#
# Two assertions, so the test is meaningful even when the underlying move
# happens to preserve permissions (same-filesystem rename):
#   1. The original mode is recorded in metadata.json at quarantine time.
#   2. After restore, stat reports the original mode.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir permissions)"
FILE="$DIR/evil_exec.sh"

seed_file "$FILE" "$SIG_A"
chmod 750 "$FILE"                       # rwxr-x--- : distinctive, not a default
BEFORE="$(stat -c '%a' "$FILE")"
BEFORE_DEC="$((8#$BEFORE))"             # e.g. 750(oct) -> 488(dec) as stored in JSON
echo "original mode: $BEFORE (decimal $BEFORE_DEC)"

av scan "$DIR" >/dev/null

# (1) Permissions were recorded in the metadata.
META="$RUNDIR/runtime/quarantine/metadata.json"
grep -q "\"permissions\": $BEFORE_DEC" "$META" \
    || fail "metadata did not record permissions=$BEFORE_DEC ($(cat "$META"))"

ID="$(av q-list | awk '/ID:/ {print $2; exit}')"
[[ -n "$ID" ]] || fail "could not read a quarantine id"
[[ ! -e "$FILE" ]] || fail "file was not moved into quarantine"

av restore "$ID"

# (2) The file came back with its original mode.
[[ -f "$FILE" ]] || fail "file was not restored"
AFTER="$(stat -c '%a' "$FILE")"
echo "restored mode: $AFTER"
[[ "$AFTER" == "$BEFORE" ]] || fail "permissions not preserved: before=$BEFORE after=$AFTER"
