#!/bin/bash
# REAL whole-machine scan-all. Unlike test_14/test_15, this does NOT restrict
# the exclude list, so `scan-all` genuinely walks "/" (tens of thousands of
# files) with only the built-in system excludes (/proc /sys /dev /run /tmp) and
# the scanner's self-exclusion of its own working directory.
#
# It runs in its OWN private runtime/config directory ($REAL_DIR), completely
# separate from the shared sandbox, so it can be part of test_main.sh without
# disturbing the other tests.
#
# Safety design so this can NEVER quarantine a pre-existing real file:
#   * The signature is a random, one-time token generated at runtime. Nothing on
#     disk can already contain it. Only the file we plant matches.
#   * The token is never echoed before/during the scan, so terminal-log capture
#     files cannot come to contain it mid-run.
#   * As a belt-and-braces guard, cleanup runs `restore-all` BEFORE its runtime
#     (which holds the quarantine) is removed, moving anything quarantined back.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
source "$HERE/test_helpers.sh"
require_bin

# Private runtime + config, separate from the shared sandbox. Because the
# scanner self-excludes its own working directory, $REAL_DIR is never scanned.
REAL_DIR="$PARENT/av_test_real_run"
rm -rf "$REAL_DIR"
mkdir -p \
    "$REAL_DIR/config" \
    "$REAL_DIR/runtime/cache" \
    "$REAL_DIR/runtime/quarantine/files" \
    "$REAL_DIR/runtime/resume" \
    "$REAL_DIR/runtime/logs"

# Run the scanner from $REAL_DIR so it uses this private config/runtime.
real_av() { ( cd "$REAL_DIR" && "$BIN" "$@" ); }

# A random, one-time signature. Guaranteed not to exist anywhere on disk yet.
RUN_ID="$(cat /proc/sys/kernel/random/uuid 2>/dev/null | tr -d '-')"
[[ -n "$RUN_ID" ]] || RUN_ID="$(date +%s%N)$$"
SIG="__AV_REAL_SCAN_ONETIME_${RUN_ID}__"
printf '%s\n' "$SIG" > "$REAL_DIR/config/signatures.txt"
# Exclude only this test's private runtime/config tree so quarantine copies and
# logs (which contain the one-time signature after the first hit) are not
# re-scanned as extra "malicious" files. The machine itself stays unrestricted.
printf '%s\n' "$REAL_DIR" > "$REAL_DIR/config/exclude.txt"

# Plant one malicious file in a real, non-excluded location.
EVIL_DIR="$DATA/real_scanall"
rm -rf "$EVIL_DIR"
mkdir -p "$EVIL_DIR"
printf '%s\n' "$SIG" > "$EVIL_DIR/planted_evil.txt"

cleanup() {
    echo "=== cleanup ==="
    # Undo EVERY quarantine before deleting the private runtime, so nothing real
    # is lost even in the impossible case that a signature matched a real file.
    real_av restore-all >/dev/null 2>&1 || true
    rm -rf "$EVIL_DIR" "$REAL_DIR"
    echo "cleanup done"
}
trap cleanup EXIT

echo "Running a REAL scan-all over / (this can take a while)..."
OUT="$(real_av scan-all)"
echo "$OUT"

grep -q "Scan summary:" <<< "$OUT" || fail "scan-all produced no summary"

scanned="$(summary_field "$OUT" Scanned)"
malicious="$(summary_field "$OUT" Malicious)"

# A genuine full scan must touch a substantial number of files.
[[ "${scanned:-0}" -ge 1000 ]] || fail "real scan-all scanned suspiciously few files (scanned='$scanned')"

# The one-time signature can only match the file we planted: exactly one hit.
[[ "${malicious:-0}" -eq 1 ]] || fail "expected exactly 1 malicious file, saw '$malicious'"
if ! real_av q-list | grep -q "planted_evil.txt"; then
    fail "planted malicious file was not quarantined"
fi

echo "Real scan-all OK: scanned=$scanned malicious=$malicious"
