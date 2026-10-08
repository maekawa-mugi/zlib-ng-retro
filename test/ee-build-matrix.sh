#!/bin/sh
# Host-side PS2 Linux build matrix. Copy the resulting bundle to the PS2
# once, then execute run-all.sh; do not compile on the target.
set -eu
if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
    echo "Usage: $0 /absolute/path/to/ee-toolchain.cmake [output-dir] [build-root]" >&2
    exit 2
fi
TOOLCHAIN=$1
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd)
ROOT=$(CDPATH= cd "$HERE/.." && pwd)
OUT=${2:-"$ROOT/ee-mmi-bundle"}
WORK=${3:-"$ROOT/build/ee-mmi-matrix"}
if [ ! -f "$TOOLCHAIN" ]; then
    echo "Toolchain file does not exist: $TOOLCHAIN" >&2
    exit 2
fi
mkdir -p "$OUT" "$WORK"
printf 'variant,feature,elf\n' > "$OUT/manifest.csv"

build_one() {
    label=$1
    feature=$2
    shift 2
    builddir="$WORK/$label"
    echo "Building $label ($feature) ..."
    cmake -S "$ROOT" -B "$builddir" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DWITH_MMI=ON -DWITH_MMI_COMPARE64=ON \
        -DWITH_MMI_ADLER32=ON \
        -DWITH_MMI_CHORBA=ON -DWITH_CRC32_CHORBA=ON \
        -DWITH_MMI_COMPARE_SWAR=OFF \
        -DWITH_MMI_SLIDE_HASH_INTERLEAVED=OFF \
        -DWITH_MMI_CHUNKSET_BURST=OFF \
        -DWITH_MMI_CHUNKSET_PATTERN=OFF \
        -DWITH_MMI_ADLER32_FORMULA=OFF \
        -DWITH_MMI_ADLER32_FUSED_COPY=OFF \
        -DWITH_MMI_CHORBA_PAIRED_TAPS=OFF \
        -DWITH_MMI_CHORBA_FUSED_COPY=OFF \
        -DWITH_MMI_CHORBA_THRESHOLD=4096 \
        -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=ON \
        -DWITH_GTEST=OFF -DZLIB_COMPAT=ON \
        "$@"
    cmake --build "$builddir" --target mmi_suite
    if [ ! -f "$builddir/mmi_suite" ]; then
        echo "Missing expected mmi_suite ELF at $builddir" >&2
        exit 1
    fi
    cp "$builddir/mmi_suite" "$OUT/mmi_suite_$label"
    printf '%s,%s,%s\n' "$label" "$feature" "mmi_suite_$label" >> "$OUT/manifest.csv"
}

# Each experiment changes one feature compared with the control. The
# original MMI, Adler, and Chorba parents are deliberately kept ON in
# every build; their comparisons are inside the suite executable.
build_one baseline NONE
build_one compare_swar WITH_MMI_COMPARE_SWAR -DWITH_MMI_COMPARE_SWAR=ON
build_one slide_interleaved WITH_MMI_SLIDE_HASH_INTERLEAVED -DWITH_MMI_SLIDE_HASH_INTERLEAVED=ON
build_one chunkset_burst WITH_MMI_CHUNKSET_BURST -DWITH_MMI_CHUNKSET_BURST=ON
build_one chunkset_pattern WITH_MMI_CHUNKSET_PATTERN -DWITH_MMI_CHUNKSET_PATTERN=ON
build_one adler_formula WITH_MMI_ADLER32_FORMULA -DWITH_MMI_ADLER32_FORMULA=ON
build_one adler_fused_copy WITH_MMI_ADLER32_FUSED_COPY -DWITH_MMI_ADLER32_FUSED_COPY=ON
build_one chorba_paired WITH_MMI_CHORBA_PAIRED_TAPS -DWITH_MMI_CHORBA_PAIRED_TAPS=ON
build_one chorba_fused_copy WITH_MMI_CHORBA_FUSED_COPY -DWITH_MMI_CHORBA_FUSED_COPY=ON
build_one chorba_threshold_1024 WITH_MMI_CHORBA_THRESHOLD -DWITH_MMI_CHORBA_THRESHOLD=1024
build_one chorba_threshold_8192 WITH_MMI_CHORBA_THRESHOLD -DWITH_MMI_CHORBA_THRESHOLD=8192

# The combined run measures the whole-stream effect of adopting all
# candidates simultaneously; it is NOT a replacement for one-variable A/B.
build_one combined ALL \
    -DWITH_MMI_COMPARE_SWAR=ON \
    -DWITH_MMI_SLIDE_HASH_INTERLEAVED=ON \
    -DWITH_MMI_CHUNKSET_BURST=ON \
    -DWITH_MMI_CHUNKSET_PATTERN=ON \
    -DWITH_MMI_ADLER32_FORMULA=ON \
    -DWITH_MMI_ADLER32_FUSED_COPY=ON \
    -DWITH_MMI_CHORBA_PAIRED_TAPS=ON \
    -DWITH_MMI_CHORBA_FUSED_COPY=ON

cp "$HERE/ee-run-matrix.sh" "$OUT/run-all.sh"
chmod +x "$OUT/run-all.sh"
echo "Matrix complete: $OUT"
echo "Transfer this entire directory to the PS2 Linux filesystem once."
