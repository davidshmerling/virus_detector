#!/bin/bash
# Generation prune must NOT run if scan-all crashes mid-run. After a successful
# scan-all caches two files, deleting one from disk and killing the next
# scan-all must leave the deleted file's cache row intact (prune only happens
# after a completed scan-all reaches commitGeneration).
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

SCANROOT="/av_scanall_test"
EXCLUDE="$RUNDIR/config/exclude.txt"
CACHE_DB="$RUNDIR/runtime/cache/cache.db"
CHECKPOINT="$RUNDIR/runtime/resume/checkpoint.txt"
KEEP="$SCANROOT/keep.txt"
GONE="$SCANROOT/gone.txt"

reset_cache
reset_quarantine
reset_resume

BACKUP="$(mktemp)"
cp "$EXCLUDE" "$BACKUP"
trap 'cp "$BACKUP" "$EXCLUDE"; rm -f "$BACKUP"; rm -rf "$SCANROOT"' EXIT

rm -rf "$SCANROOT"
mkdir -p "$SCANROOT"
seed_file "$KEEP" "survives"
seed_file "$GONE" "deleted before incomplete scan-all"

# Enough files that scan-all stays in "running" long enough to kill mid-flight.
for i in $(seq 1 40000); do
    printf 'pad %s\n' "$i" > "$SCANROOT/pad_$i.txt"
done

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

echo "--- first scan-all (complete) ---"
OUT1="$(av scan-all)"
echo "$OUT1"
grep -q "Scan summary:" <<< "$OUT1" || fail "first scan-all produced no summary"
[[ "$(cache_has "$KEEP")" -eq 1 ]] || fail "keep.txt missing from cache"
[[ "$(cache_has "$GONE")" -eq 1 ]] || fail "gone.txt missing from cache"

rm -f "$GONE"
[[ ! -e "$GONE" ]] || fail "failed to delete gone.txt"
reset_resume

echo "--- second scan-all (killed mid-run) ---"
( cd "$RUNDIR" && exec "$BIN" scan-all ) >/dev/null 2>&1 &
PID=$!

# Wait until the scan has actually started, then interrupt it.
for _ in $(seq 1 200); do
    if [[ -f "$CHECKPOINT" ]] && [[ "$(sed -n '2p' "$CHECKPOINT")" == "running" ]]; then
        break
    fi
    sleep 0.01
done

[[ -f "$CHECKPOINT" ]] || fail "checkpoint never appeared"
[[ "$(sed -n '2p' "$CHECKPOINT")" == "running" ]] \
    || fail "scan finished before kill; cannot assert incomplete prune"

kill -KILL "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

[[ "$(sed -n '2p' "$CHECKPOINT")" == "running" ]] \
    || fail "checkpoint left 'completed' after kill (prune may have run)"

[[ "$(cache_has "$KEEP")" -eq 1 ]] || fail "keep.txt was dropped after crash"
[[ "$(cache_has "$GONE")" -eq 1 ]] \
    || fail "gone.txt was pruned after incomplete scan-all (should remain)"

echo "Incomplete scan-all correctly left cache entries unpruned."
