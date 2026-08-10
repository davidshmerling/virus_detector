#!/bin/bash
# An unreadable file (chmod 000) must count as Failed, not Clean: no crash,
# Failed >= 1, Malicious 0, and the file must not be quarantined or cached as
# Clean.
#
# When the suite runs as root, DAC mode bits are ignored for the scanner
# process — so we drop to `nobody` for the scan (root still owns the 000 file).
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir unreadable)"
LOCKED="$DIR/locked.txt"
seed_file "$LOCKED" "harmless content"
seed_file "$DIR/readable.txt" "also harmless"

# Run av; as root, drop to nobody so chmod 000 actually blocks open().
scan_as_nobody_if_root() {
    if [[ "$(id -u)" -eq 0 ]] && id nobody >/dev/null 2>&1; then
        chown -R nobody:nogroup "$RUNDIR"
        ( cd "$RUNDIR" && runuser -u nobody -- "$BIN" "$@" )
    else
        av "$@"
    fi
}

chmod a+rX "$DATA" "$DIR"
chmod 644 "$DIR/readable.txt"
chmod 000 "$LOCKED"
trap 'chmod u+rw "$LOCKED" 2>/dev/null || true' EXIT

OUT="$(scan_as_nobody_if_root scan "$DIR" 2>&1)"
echo "$OUT"

failed="$(summary_field "$OUT" Failed)"
malicious="$(summary_field "$OUT" Malicious)"
quarantined="$(summary_field "$OUT" Quarantined)"
scanned="$(summary_field "$OUT" Scanned)"

[[ "${failed:-0}" -ge 1 ]] || fail "expected Failed >= 1, got '$failed'"
[[ "${malicious:-x}" == "0" ]] || fail "unreadable file must not be malicious"
[[ "${quarantined:-x}" == "0" ]] || fail "unreadable file must not be quarantined"
[[ "${scanned:-0}" -ge 1 ]] || fail "readable sibling should still be scanned"
[[ -f "$LOCKED" ]] || fail "unreadable file disappeared"

# Restore perms so locked.txt can be scanned on the next run.
chmod 644 "$LOCKED"

OUT2="$(scan_as_nobody_if_root scan "$DIR")"
echo "--- second ---"
echo "$OUT2"
scanned2="$(summary_field "$OUT2" Scanned)"
failed2="$(summary_field "$OUT2" Failed)"
[[ "${failed2:-x}" == "0" ]] || fail "after restore, Failed should be 0 (got '$failed2')"
[[ "${scanned2:-0}" -ge 1 ]] || fail "locked.txt should be scanned after chmod restore"
