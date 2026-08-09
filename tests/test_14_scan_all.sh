#!/bin/bash
# Exercises the `scan-all` command, which is hardcoded to scan "/".
#
# To keep this fast and incapable of touching real files, we plant the target
# at a top-level directory (/av_scanall_test) and rewrite the sandbox exclude
# list to skip every OTHER entry under "/". The walk therefore only descends
# into our directory. The ultra-unique signatures are a second safety net:
# even if the exclusion missed something, no real file could match them.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

SCANROOT="/av_scanall_test"
EXCLUDE="$RUNDIR/config/exclude.txt"

reset_cache
reset_quarantine

# Restore the exclude file for any later test, even if we fail midway.
BACKUP="$(mktemp)"
cp "$EXCLUDE" "$BACKUP"
trap 'cp "$BACKUP" "$EXCLUDE"; rm -f "$BACKUP"; rm -rf "$SCANROOT"' EXIT

rm -rf "$SCANROOT"
mkdir -p "$SCANROOT"
seed_file "$SCANROOT/clean.txt" "nothing to see here"
seed_file "$SCANROOT/evil.txt" "$SIG_A"

# Exclude every top-level entry except our scan root. Cover dot-entries too
# (e.g. /.dockerenv) so the walk touches nothing but our directory.
: > "$EXCLUDE"
shopt -s dotglob
for entry in /*; do
    case "$entry" in
        /.|/..|"$SCANROOT") continue ;;
    esac
    printf '%s\n' "$entry" >> "$EXCLUDE"
done
shopt -u dotglob

OUT="$(av scan-all)"
echo "$OUT"

grep -q "Scan summary:" <<< "$OUT" || fail "scan-all produced no summary"

malicious="$(summary_field "$OUT" Malicious)"
[[ "${malicious:-0}" -ge 1 ]] || fail "scan-all did not detect the planted signature (malicious='$malicious')"

QOUT="$(av q-list)"
grep -q "evil.txt"  <<< "$QOUT" || fail "scan-all did not quarantine evil.txt"
if grep -q "clean.txt" <<< "$QOUT"; then
    fail "scan-all wrongly quarantined clean.txt"
fi
