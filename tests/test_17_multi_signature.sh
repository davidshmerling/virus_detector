#!/bin/bash
# A single file that contains five different signatures must be detected once,
# and ALL five matched signatures must be reported for it in the quarantine
# listing. This exercises the Aho-Corasick automaton finding multiple distinct
# patterns in one pass.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir multi_signature)"
FILE="$DIR/dangerous.txt"

{
    printf 'harmless intro line\n'
    printf '%s\n' "$SIG_A"
    printf 'some filler in between\n'
    printf '%s\n' "$SIG_B"
    printf '%s\n' "$SIG_C"
    printf 'more harmless text\n'
    printf '%s\n' "$SIG_D"
    printf '%s\n' "$SIG_E"
    printf 'trailing text\n'
} > "$FILE"

OUT="$(av scan "$DIR")"
echo "$OUT"

malicious="$(summary_field "$OUT" Malicious)"
[[ "${malicious:-0}" -eq 1 ]] || fail "expected exactly one malicious file, saw '$malicious'"

QOUT="$(av q-list)"
echo "$QOUT"

grep -q "dangerous.txt" <<< "$QOUT" || fail "dangerous.txt was not quarantined"

for sig in "$SIG_A" "$SIG_B" "$SIG_C" "$SIG_D" "$SIG_E"; do
    grep -qF "$sig" <<< "$QOUT" || fail "signature not reported in q-list: $sig"
done

echo "All five signatures were detected and reported."
