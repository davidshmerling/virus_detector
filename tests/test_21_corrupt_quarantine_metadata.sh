#!/bin/bash
# Corrupt quarantine metadata.json: load() clears in-memory state and returns
# false. Quarantine CLI commands must fail cleanly (non-zero) without pretending
# there are entries.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_quarantine
mkdir -p "$RUNDIR/runtime/quarantine"
printf '{ not valid json\n' > "$RUNDIR/runtime/quarantine/metadata.json"

set +e
OUT="$(av q-list 2>&1)"
RC=$?
set -e

echo "$OUT"
[[ "$RC" -ne 0 ]] || fail "q-list succeeded on corrupt metadata (expected failure)"
grep -qiE 'quarantine|corrupt|Could not open' <<< "$OUT" \
    || fail "q-list gave no clear error on corrupt metadata"
# Must not print a fake entry list as if load succeeded.
grep -qE '\.txt|/.*quarantine' <<< "$OUT" && fail "q-list looked like it listed entries" || true
