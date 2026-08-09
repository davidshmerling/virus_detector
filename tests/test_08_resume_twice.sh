#!/bin/bash
# Harsher resume test: interrupt the same scan twice, then let it finish.
#   run -> kill -> run -> kill -> run to completion.
# After every kill a checkpoint must exist, and the final run must complete.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

CHECKPOINT="$RUNDIR/runtime/resume/checkpoint.txt"

reset_cache
reset_resume

for attempt in 1 2; do
    ( cd "$RUNDIR" && exec "$BIN" scan "$DATA/resume" ) >/dev/null 2>&1 &
    PID=$!
    sleep 0.12
    kill -INT "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true

    [[ -f "$CHECKPOINT" ]] || fail "no checkpoint after kill #$attempt"
    echo "after kill #$attempt: status=$(sed -n '2p' "$CHECKPOINT")"
done

av scan "$DATA/resume" >/dev/null

FINAL_STATUS="$(sed -n '2p' "$CHECKPOINT")"
echo "final checkpoint status: $FINAL_STATUS"
[[ "$FINAL_STATUS" == "completed" ]] || fail "scan did not reach 'completed' (was '$FINAL_STATUS')"
