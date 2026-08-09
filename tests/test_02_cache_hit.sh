#!/bin/bash
# The second scan of an unchanged directory must be served entirely from cache:
# first run scans everything (Cached 0), second run scans nothing (Cached > 0).
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

reset_cache

FIRST="$(av scan "$DATA/clean")"
SECOND="$(av scan "$DATA/clean")"

echo "--- first ---";  echo "$FIRST"
echo "--- second ---"; echo "$SECOND"

first_cached="$(summary_field "$FIRST" Cached)"
second_cached="$(summary_field "$SECOND" Cached)"
second_scanned="$(summary_field "$SECOND" Scanned)"

[[ "${first_cached:-x}" == "0" ]]   || fail "first scan should have 0 cached, got '$first_cached'"
[[ "${second_cached:-0}" -ge 1 ]]   || fail "second scan should reuse cache, cached='$second_cached'"
[[ "${second_scanned:-x}" == "0" ]] || fail "second scan should scan nothing, scanned='$second_scanned'"
