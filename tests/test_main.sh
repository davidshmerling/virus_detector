#!/bin/bash
# Runs the whole integration suite as a transaction:
#
#   ./tests/test_main.sh
#
#   1. Setup   - build test data + a sandbox with dedicated test config and a
#                throwaway runtime, all OUTSIDE the repo.
#   2. Run     - execute every test_NN_*.sh against that sandbox.
#   3. Cleanup - remove the sandbox and test data.
#
# `trap ... EXIT` guarantees step 3 runs even if a test fails or the script is
# aborted by set -e / a signal. The user's real config/ and runtime/ are never
# touched, so there is nothing to back up or restore.
set -uo pipefail

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$TEST_DIR/test_helpers.sh"
require_bin

# Registered BEFORE any test data or sandbox is created, so cleanup runs no
# matter where we exit.
trap '"$TEST_DIR/cleanup_test_env.sh" || true' EXIT

"$TEST_DIR/setup_test_env.sh"

PASSED=0
FAILED=0
FAILED_TESTS=()

for test in "$TEST_DIR"/test_*.sh; do
    name="$(basename "$test")"
    case "$name" in
        test_main.sh|test_helpers.sh) continue ;;
    esac

    echo
    echo "=================================="
    echo "Running: $name"
    echo "=================================="

    if bash "$test"; then
        echo "PASS: $name"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: $name"
        FAILED=$((FAILED + 1))
        FAILED_TESTS+=("$name")
    fi
done

echo
echo "=================================="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
if [[ ${#FAILED_TESTS[@]} -gt 0 ]]; then
    printf '  - %s\n' "${FAILED_TESTS[@]}"
fi
echo "=================================="

exit "$FAILED"
