#!/usr/bin/env bash
# Usage: PS2DEV=... PS2SDK=... bash test/ps2/build.sh [output-dir] [cmake-options...]
set -euo pipefail
source_dir=$(cd "$(dirname "$0")/../.." && pwd)
build_jobs=${BUILD_JOBS:-${JOBS:-$(nproc)}}
(( build_jobs >= 1 )) || { echo "BUILD_JOBS must be >= 1" >&2; exit 2; }
output_dir=${1:-"$source_dir/build-ps2-mmi"}
if (( $# )); then shift; fi
cmake -S "$source_dir" -B "$output_dir" \
    -DCMAKE_TOOLCHAIN_FILE="$source_dir/cmake/toolchain-ps2-ee.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_MMI=ON -DWITH_MMI_COMPARE64=ON \
    -DWITH_MMI_ADLER32=ON -DWITH_MMI_CHORBA=ON \
    -DWITH_CRC32_CHORBA=ON -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=ON -DWITH_GTEST=OFF -DWITH_GZFILEOP=OFF \
    -DWITH_PS2_TEST_RUNNER=ON "$@"
cmake --build "$output_dir" --target mmi_suite -j "$build_jobs"
# Remove leftovers from older two-ELF builds, not user-created files.
rm -f "$output_dir/zlib_ng_mmi_test_only.elf"
printf '\nELF: %s/zlib_ng_mmi.elf (parallel jobs=%s; test-only omitted)\n' "$output_dir" "$build_jobs"
