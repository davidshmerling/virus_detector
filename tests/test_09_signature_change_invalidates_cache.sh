#!/bin/bash
# A cache entry records the signatures file's modification time. Changing that
# file must invalidate the cache so every file is re-scanned. We only bump the
# mtime (touch) of the sandbox's private copy; the real config is untouched.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

SIGFILE="$RUNDIR/config/signatures.txt"

reset_cache

av scan "$DATA/clean" >/dev/null                # populate cache
WARM="$(av scan "$DATA/clean")"                 # should be a full cache hit
warm_cached="$(summary_field "$WARM" Cached)"
[[ "${warm_cached:-0}" -ge 1 ]] || fail "cache was not warm before the change (cached='$warm_cached')"

# Bump the signatures mtime (content unchanged). sleep guards filesystems with
# coarse (1s) timestamp resolution.
sleep 1
touch "$SIGFILE"

AFTER="$(av scan "$DATA/clean")"
echo "$AFTER"
after_cached="$(summary_field "$AFTER" Cached)"
after_scanned="$(summary_field "$AFTER" Scanned)"

[[ "${after_cached:-x}" == "0" ]] || fail "cache not invalidated after signature change (cached='$after_cached')"
[[ "${after_scanned:-0}" -ge 1 ]] || fail "files not re-scanned after signature change (scanned='$after_scanned')"
