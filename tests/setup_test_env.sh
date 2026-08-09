#!/bin/bash
# Prepares the isolated sandbox ($RUNDIR: private config/ + fresh runtime/) and
# builds the shared data tree under $DATA. Both live OUTSIDE the repo, never
# inside it. Tests that mutate files create their own private subdirectories,
# so they stay independent of one another and of run order.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"

init_sandbox

echo "Sandbox (dedicated config + throwaway runtime): $RUNDIR"
echo "Wrote test signatures and exclude rules into: $RUNDIR/config"
echo "Building test data under: $DATA"
rm -rf "$DATA"
mkdir -p "$DATA/clean/nested" "$DATA/resume"

printf 'hello world\n'           > "$DATA/clean/a.txt"
printf 'completely clean file\n' > "$DATA/clean/b.txt"
printf 'nested clean\n'          > "$DATA/clean/nested/c.txt"

# Many small files so an interrupted scan still has work left to resume.
count="${RESUME_FILE_COUNT:-8000}"
for i in $(seq 1 "$count"); do
    printf 'resume test file %s\n' "$i" > "$DATA/resume/file_$i.txt"
done

echo "Created $count resume files."
echo "Test environment ready."
