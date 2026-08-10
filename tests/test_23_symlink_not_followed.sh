#!/bin/bash
# Symlinks are skipped (not followed). A symlink pointing at a malicious file
# outside the scan root must not cause that file to be scanned or quarantined.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir symlink_skip)"
OUTSIDE="$(fresh_dir symlink_target)"

seed_file "$OUTSIDE/secret_malicious.txt" "$SIG_A"
seed_file "$DIR/clean.txt" "harmless"
ln -s "$OUTSIDE/secret_malicious.txt" "$DIR/link_to_secret"

OUT="$(av scan "$DIR")"
echo "$OUT"

quarantined="$(summary_field "$OUT" Quarantined)"
malicious="$(summary_field "$OUT" Malicious)"

[[ "${quarantined:-x}" == "0" ]] || fail "symlink was followed (quarantined='$quarantined')"
[[ "${malicious:-x}" == "0" ]] || fail "symlink was followed (malicious='$malicious')"
[[ -f "$OUTSIDE/secret_malicious.txt" ]] \
    || fail "target of symlink was moved/quarantined"
[[ -L "$DIR/link_to_secret" ]] || fail "symlink itself disappeared"
