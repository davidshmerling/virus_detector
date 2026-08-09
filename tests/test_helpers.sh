#!/bin/bash
# Shared helpers for the av_scanner integration tests.
#
# Isolation model
# ---------------
# The scanner resolves config/ and runtime/ RELATIVE TO ITS WORKING DIRECTORY.
# We exploit that to run every test in a private sandbox ($RUNDIR) that has its
# own DEDICATED test config/ and a throwaway runtime/. The tests therefore never
# touch the user's real runtime/ (which may hold genuinely quarantined files) or
# the real config/. Everything the tests create lives outside the repo tree and
# is removed on exit.
#
# av() is the only supported way to invoke the binary: it runs from $RUNDIR.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/av"

# Test data and the sandbox both live OUTSIDE the repository. The scanner
# automatically excludes its own project root (the dir containing .git) plus
# the system dirs /proc /sys /dev /run /tmp, so data placed inside the repo or
# under /tmp would silently never be scanned. Siblings of the repo avoid both.
PARENT="$(dirname "$ROOT")"
DATA="${AV_TEST_DATA:-$PARENT/av_test_data}"
RUNDIR="${AV_TEST_RUNDIR:-$PARENT/av_test_run}"

# Dedicated test signatures written into the sandbox's config. They are
# intentionally long, prefixed, and randomised so there is essentially no chance
# they appear in any file we did not create ourselves -- important because
# test_14 exercises `scan-all`, which walks a much larger tree.
SIG_A="__AV_TEST_SIGNATURE_ALPHA_1f4b9c2e7a6d0853__DO_NOT_MATCH_ANYWHERE__"
SIG_B="__AV_TEST_SIGNATURE_BETA_9e3d7c1a5b2f8046__DO_NOT_MATCH_ANYWHERE__"
SIG_C="__AV_TEST_SIGNATURE_GAMMA_5a1b2c3d4e5f6070__DO_NOT_MATCH_ANYWHERE__"
SIG_D="__AV_TEST_SIGNATURE_DELTA_6b2c3d4e5f607181__DO_NOT_MATCH_ANYWHERE__"
SIG_E="__AV_TEST_SIGNATURE_EPSILON_7c3d4e5f60718292__DO_NOT_MATCH_ANYWHERE__"
SIG_CHUNK="__AV_TEST_SIGNATURE_CHUNK_c0ffee1234deadbeef__CROSSES_CHUNK_BOUNDARY__"

require_bin() {
    if [[ ! -x "$BIN" ]]; then
        echo "Binary not found at $BIN. Build it first with: make" >&2
        exit 1
    fi
}

fail() {
    echo "ASSERT FAILED: $*" >&2
    exit 1
}

# Create/refresh the isolated sandbox: a DEDICATED test config/ and an empty
# runtime/. The real config/ and runtime/ are never read or written. Safe to
# call repeatedly.
init_sandbox() {
    mkdir -p \
        "$RUNDIR/config" \
        "$RUNDIR/runtime/cache" \
        "$RUNDIR/runtime/quarantine/files" \
        "$RUNDIR/runtime/resume" \
        "$RUNDIR/runtime/logs"

    # Dedicated test signatures (one per line; '#' and blanks are ignored).
    cat > "$RUNDIR/config/signatures.txt" <<EOF
# Test-only signatures. Not the real signature set.
$SIG_A
$SIG_B
$SIG_C
$SIG_D
$SIG_E
$SIG_CHUNK
EOF

    # Dedicated exclude set. test_12_excluded_paths relies on this entry
    # (absolute paths only, as the loader requires).
    cat > "$RUNDIR/config/exclude.txt" <<EOF
# Test-only exclude rules.
$DATA/exclude
EOF
}

# Run the scanner inside the sandbox so config/ and runtime/ are the private
# copies, never the user's real ones.
av() {
    ( cd "$RUNDIR" && "$BIN" "$@" )
}

# seed_file <path> <content-line>
seed_file() {
    mkdir -p "$(dirname "$1")"
    printf '%s\n' "$2" > "$1"
}

# fresh_dir <name>  ->  prints an empty, freshly created data subdirectory.
fresh_dir() {
    local dir="$DATA/$1"
    rm -rf "$dir"
    mkdir -p "$dir"
    printf '%s' "$dir"
}

# summary_field "<scan output>" <Name>   ->   prints the count for that row.
# The scan summary rows look like:  "  Cached:      12"
summary_field() {
    awk -v key="$2:" '$1 == key { print $2; exit }' <<< "$1"
}

reset_cache() {
    rm -f "$RUNDIR"/runtime/cache/cache.db*
}

reset_quarantine() {
    rm -f "$RUNDIR"/runtime/quarantine/metadata.json
    rm -rf "$RUNDIR"/runtime/quarantine/files
    mkdir -p "$RUNDIR"/runtime/quarantine/files
}

reset_resume() {
    rm -f "$RUNDIR"/runtime/resume/checkpoint.txt
}
