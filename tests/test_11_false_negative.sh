#!/bin/bash
# The important cross-chunk test. Files are read in 1 MiB chunks; the automaton
# state must persist across chunk boundaries. We plant a real signature so that
# it straddles the 1 MiB boundary (a few bytes in chunk 1, the rest in chunk 2)
# and assert it is still detected. A naive per-chunk scanner would MISS this.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache
reset_quarantine
DIR="$(fresh_dir chunk_boundary)"
FILE="$DIR/straddle.bin"

CHUNK=1048576          # 1 MiB, must match FileProcessor::kChunkSize
# 8 bytes of the signature land at the end of chunk 1, the rest in chunk 2.
head -c $((CHUNK - 8)) /dev/zero | tr '\0' 'A' > "$FILE"
printf '%s' "$SIG_CHUNK" >> "$FILE"
head -c 4096 /dev/zero | tr '\0' 'A' >> "$FILE"

OUT="$(av scan "$DIR")"
echo "$OUT"

malicious="$(summary_field "$OUT" Malicious)"
[[ "${malicious:-0}" -ge 1 ]] || fail "signature across a chunk boundary was NOT detected"

if ! av q-list | grep -q "straddle.bin"; then
    fail "boundary-straddling file was not quarantined"
fi
