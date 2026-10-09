#!/bin/sh
# PS2 Linux /bin/sh: run every prebuilt EE suite ELF in this directory.
# Use no Python, network access, or additional packages on the PS2.
set -u
case ${1:---all} in
    --all|--tests|--benches|--smoke) mode=${1:---all} ;;
    *)
        echo "Usage: $0 [--all|--tests|--benches|--smoke]" >&2
        exit 2 ;;
esac
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd)
LOG_DIR="$HERE/results"
mkdir -p "$LOG_DIR" || exit 1
tag=${mode#--}
passed=0
failed=0
for exe in "$HERE"/mmi_suite_*; do
    [ -f "$exe" ] || continue
    name=${exe##*/}
    log="$LOG_DIR/${name}_${tag}.log"
    echo "RUN $name $mode => $log"
    if "$exe" "$mode" > "$log" 2>&1; then
        echo "PASS $name"
        passed=$((passed + 1))
    else
        echo "FAIL $name (see $log)"
        failed=$((failed + 1))
    fi
    grep '^MMI_SUITE_SUMMARY,' "$log" || true
done
echo "PS2_MATRIX_SUMMARY,$tag,$passed,$failed"
[ "$passed" -ne 0 ] && [ "$failed" -eq 0 ]
