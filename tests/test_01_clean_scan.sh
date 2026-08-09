#!/bin/bash
# A directory of harmless files must scan cleanly: something is scanned, and
# nothing is flagged malicious.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache

OUT="$(av scan "$DATA/clean")"
echo "$OUT"

scanned="$(summary_field "$OUT" Scanned)"
malicious="$(summary_field "$OUT" Malicious)"

[[ "${scanned:-0}" -ge 1 ]]    || fail "expected at least one file scanned, got '$scanned'"
[[ "${malicious:-x}" == "0" ]] || fail "clean files were flagged malicious ($malicious)"
