#!/bin/bash
# After a successful scan-all, a file that disappeared from disk must be dropped
# from the cache (generation cleanup). Flow:
#   1. scan-all with two clean files under a restricted root
#   2. delete one of them
#   3. scan-all again
#   4. assert the surviving file is still cached and the deleted one is gone
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

SCANROOT="/av_scanall_test"
EXCLUDE="$RUNDIR/config/exclude.txt"
CACHE_DB="$RUNDIR/runtime/cache/cache.db"
KEEP="$SCANROOT/keep.txt"
GONE="$SCANROOT/gone.txt"

reset_cache
reset_quarantine

BACKUP="$(mktemp)"
cp "$EXCLUDE" "$BACKUP"
trap 'cp "$BACKUP" "$EXCLUDE"; rm -f "$BACKUP"; rm -rf "$SCANROOT"' EXIT

rm -rf "$SCANROOT"
mkdir -p "$SCANROOT"
seed_file "$KEEP" "still here after both scans"
seed_file "$GONE" "will be deleted between scans"

# Restrict scan-all to our planted directory only (same pattern as test_14).
: > "$EXCLUDE"
shopt -s dotglob
for entry in /*; do
    case "$entry" in
        /.|/..|"$SCANROOT") continue ;;
    esac
    printf '%s\n' "$entry" >> "$EXCLUDE"
done
shopt -u dotglob

cache_has() {
    local path="$1"
    [[ -f "$CACHE_DB" ]] || { echo 0; return; }
    sqlite3 "$CACHE_DB" \
        "SELECT COUNT(*) FROM cache_entries WHERE path = '$path';"
}

echo "--- first scan-all ---"
OUT1="$(av scan-all)"
echo "$OUT1"
grep -q "Scan summary:" <<< "$OUT1" || fail "first scan-all produced no summary"

[[ "$(cache_has "$KEEP")" -eq 1 ]] || fail "keep.txt missing from cache after first scan"
[[ "$(cache_has "$GONE")" -eq 1 ]] || fail "gone.txt missing from cache after first scan"

rm -f "$GONE"
[[ ! -e "$GONE" ]] || fail "failed to delete gone.txt from disk"

echo "--- second scan-all (after deleting gone.txt) ---"
OUT2="$(av scan-all)"
echo "$OUT2"
grep -q "Scan summary:" <<< "$OUT2" || fail "second scan-all produced no summary"

[[ "$(cache_has "$KEEP")" -eq 1 ]] || fail "keep.txt was wrongly dropped from cache"
[[ "$(cache_has "$GONE")" -eq 0 ]] || fail "gone.txt is still in cache after scan-all cleanup"

echo "Cache correctly dropped the deleted file after scan-all."
