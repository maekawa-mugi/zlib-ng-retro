#!/bin/sh
set -eu
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd)
: "${CC:=cc}"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I"$HERE" \
  "$HERE/bench_rank.c" "$HERE/test_bench_rank_host.c" -o /tmp/ps2_mmi_bench_rank_test
/tmp/ps2_mmi_bench_rank_test
