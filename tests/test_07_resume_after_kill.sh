#!/bin/bash
# Interrupt a scan, then re-run it. There is NO SIGINT handler in the program,
# so the kill terminates it immediately; the checkpoint is what makes the scan
# resumable. We assert:
#   1. a checkpoint file exists after the kill (it is written when the scan
#      begins), and
#   2. re-running the same root drives the scan to the "completed" state.
#
# The checkpoint's second line is the status (running | completed). We use
# `exec` in the background subshell so $! is the scanner's own PID and the
# signal reaches it directly.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"
require_bin

CHECKPOINT="$RUNDIR/runtime/resume/checkpoint.txt"

reset_cache
reset_resume

( cd "$RUNDIR" && exec "$BIN" scan "$DATA/resume" ) >/dev/null 2>&1 &
PID=$!
sleep 0.15
kill -INT "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

[[ -f "$CHECKPOINT" ]] || fail "no checkpoint written after interruption"
echo "checkpoint status after kill: $(sed -n '2p' "$CHECKPOINT")"

# Finish the scan (this resumes it if it was still running).
av scan "$DATA/resume" >/dev/null

FINAL_STATUS="$(sed -n '2p' "$CHECKPOINT")"
echo "checkpoint status after resume: $FINAL_STATUS"
[[ "$FINAL_STATUS" == "completed" ]] || fail "scan did not reach 'completed' (was '$FINAL_STATUS')"
