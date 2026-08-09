#!/bin/bash
# scan-all under repeated interruption: kill the full scan 10 times in a row and
# resume each time, then let it finish. Proves the checkpoint/resume machinery
# works for `scan-all` (root "/"), not just a targeted `scan <path>`.
#
# As in test_14, we plant the workload at /av_scanall_test and exclude every
# other top-level entry, so scan-all only ever descends into our directory
# (fast, and cannot touch real files). The ultra-unique signatures are a second
# safety net.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

SCANROOT="/av_scanall_test"
EXCLUDE="$RUNDIR/config/exclude.txt"
CHECKPOINT="$RUNDIR/runtime/resume/checkpoint.txt"

reset_cache
reset_quarantine
reset_resume

# Restore the exclude file and remove the scan root even if we fail midway.
BACKUP="$(mktemp)"
cp "$EXCLUDE" "$BACKUP"
trap 'cp "$BACKUP" "$EXCLUDE"; rm -f "$BACKUP"; rm -rf "$SCANROOT"' EXIT

# Enough files that a short-lived scan is still running when we kill it.
rm -rf "$SCANROOT"
mkdir -p "$SCANROOT"
count="${SCANALL_FILE_COUNT:-8000}"
for i in $(seq 1 "$count"); do
    printf 'scan-all resume file %s\n' "$i" > "$SCANROOT/file_$i.txt"
done
# One planted malicious file: it must be caught despite all the interruptions.
printf '%s\n' "$SIG_A" > "$SCANROOT/evil.txt"

# Exclude every top-level entry (incl. dot-entries) except our scan root.
: > "$EXCLUDE"
shopt -s dotglob
for entry in /*; do
    case "$entry" in
        /.|/..|"$SCANROOT") continue ;;
    esac
    printf '%s\n' "$entry" >> "$EXCLUDE"
done
shopt -u dotglob

# 10 crashes, each resuming the previous checkpoint.
for attempt in $(seq 1 10); do
    ( cd "$RUNDIR" && exec "$BIN" scan-all ) >/dev/null 2>&1 &
    PID=$!
    sleep 0.05
    kill -INT "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true

    [[ -f "$CHECKPOINT" ]] || fail "no checkpoint after crash #$attempt"
    echo "crash #$attempt -> status=$(sed -n '2p' "$CHECKPOINT") next=$(sed -n '3p' "$CHECKPOINT")"
done

# Final uninterrupted run to drive it to completion.
av scan-all >/dev/null

FINAL="$(sed -n '2p' "$CHECKPOINT")"
echo "final status: $FINAL"
[[ "$FINAL" == "completed" ]] || fail "scan-all did not complete after resume (status='$FINAL')"

# The planted malicious file must have been quarantined somewhere along the way.
if ! av q-list | grep -q "evil.txt"; then
    fail "planted malicious file was not quarantined across the resume cycles"
fi
