#!/bin/bash
# A file that merely resembles a signature but is one byte off must stay clean.
# SIG_A ends in ...123456; here we end in ...12345X.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir false_positive)"
printf 'TEST_SIGNATURE_ONE_12345X\n' > "$DIR/almost.txt"

OUT="$(av scan "$DIR")"
echo "$OUT"

malicious="$(summary_field "$OUT" Malicious)"
[[ "${malicious:-x}" == "0" ]] || fail "near-signature was flagged malicious ($malicious)"
if av q-list | grep -q "almost.txt"; then
    fail "false positive: almost.txt was quarantined"
fi
