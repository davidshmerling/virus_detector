#!/bin/bash
# Removes the test data tree and the isolated sandbox. The user's real runtime/
# and config/ are never touched by the suite, so there is nothing to restore.
set -euo pipefail
source "$(dirname "$0")/test_helpers.sh"

for target in "$DATA" "$RUNDIR"; do
    case "$target" in
        */av_test_data|*/av_test_run)
            rm -rf "$target"
            echo "Removed: $target"
            ;;
        *)
            echo "Refusing to remove unexpected path: $target" >&2
            ;;
    esac
done
