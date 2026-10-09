#!/usr/bin/env bash
# Usage: PS2DEV=... PS2SDK=... bash test/ps2/build.sh [output-dir] [cmake-options...]
set -euo pipefail
source_dir=$(cd "$(dirname "$0")/../.." && pwd)
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
cmake --build "$output_dir" --target ps2_mmi_test mmi_suite -j "${BUILD_JOBS:-4}"
printf '\nELFs: %s/ps2_mmi_test.elf and %s/mmi_suite.elf\n' "$output_dir" "$output_dir"
