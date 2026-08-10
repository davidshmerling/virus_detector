#!/bin/bash
# Excluding a directory skips that directory and never descends into its
# children — even when scanning a parent that contains both excluded and
# allowed subtrees.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir exclude_children)"
mkdir -p "$DIR/blocked" "$DIR/ok"
seed_file "$DIR/blocked/evil.txt" "$SIG_A"
seed_file "$DIR/ok/good.txt" "harmless"

EXCLUDE="$RUNDIR/config/exclude.txt"
BACKUP="$(mktemp)"
cp "$EXCLUDE" "$BACKUP"
trap 'cp "$BACKUP" "$EXCLUDE"; rm -f "$BACKUP"' EXIT

printf '%s\n' "$DIR/blocked" >> "$EXCLUDE"

OUT="$(av scan "$DIR")"
echo "$OUT"

quarantined="$(summary_field "$OUT" Quarantined)"
malicious="$(summary_field "$OUT" Malicious)"
scanned="$(summary_field "$OUT" Scanned)"

[[ "${quarantined:-x}" == "0" ]] || fail "child under excluded dir was quarantined"
[[ "${malicious:-x}" == "0" ]] || fail "child under excluded dir was flagged"
[[ -f "$DIR/blocked/evil.txt" ]] || fail "excluded child was moved away"
[[ "${scanned:-0}" -ge 1 ]] || fail "allowed sibling should still be scanned"
[[ -f "$DIR/ok/good.txt" ]] || fail "allowed file disappeared"
