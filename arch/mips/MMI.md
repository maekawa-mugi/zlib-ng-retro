# PlayStation 2 Emotion Engine MMI (experimental)

This branch adds opt-in R5900 MMI acceleration to **deflate hash-table
sliding, match comparison, and selected inflate history copies**, using
128-bit `LQ`, `SQ`, `PSUBUH`, and `PXOR` instructions. This is not MIPS
MSA, and must never be enabled for generic MIPS CPUs.

## Scope

- `slide_hash_mmi`: update both `head` and `prev` hash chains.
- `slide_hash_head_mmi`: update `head` only.
- Each MMI operation handles eight 16-bit `Pos` values in parallel using
  unsigned saturating subtraction.
- Misaligned prefixes and tails use scalar operations. All `LQ`/`SQ`
  accesses are 16-byte-aligned, since EE silently masks low address bits.
- `compare256_mmi`: use aligned MMI `LQ`/`PXOR` to find the first mismatched
  byte, with safe scalar paths for incompatible input alignments.
- MMI-backed `longest_match` variants use the same comparison routine.
- Optional `WITH_MMI_COMPARE64=ON` adds a 64-byte equality prefilter using
  `PXOR` and `POR`. A mismatch reuses the original 16-byte path to find
  its first position. This is OFF by default until real EE benchmarks.
- `chunkmemset_safe_mmi`: MMI 128-bit LZ77 history copying for aligned
  source/destination addresses and distance >= 16; generic C fallback for
  short-distance, differently aligned, or backward-overlapping copies.
- Experimental `adler32_mmi` and `adler32_copy_mmi` are available with
  `WITH_MMI_ADLER32=ON` (OFF by default). They combine MMI byte-to-halfword
  expansion and packed addition with exact scalar weighted sums, without
  PMADDH or implicit HI/LO changes. Benchmark before enabling in production.
- Optional `WITH_MMI_CHORBA=ON` enables `crc32_chorba_mmi` and
  `crc32_copy_chorba_mmi`. It is a **non-destructive** 128-bit Chorba
  implementation using `LQ/PXOR/SQ` to apply the paper's degree-44
  zero polynomial scaled by 128 over GF(2). The ring buffer is 1024
  bytes, the final remainder is 704 bytes, and a precomputed inverse
  CRC shift matrix restores the original CRC-32. Short buffers use
  `crc32_braid`; this experiment is OFF by default until PS2 testing.
- The generic
  `inflate_fast` loop is unchanged; only calls through
  `chunkmemset_safe` are redirected to the new conditional copy path.
- `compare256_mmi` uses the MMI fast path only if both input addresses
  are 16-byte aligned. Inputs with different alignment residues use
  64-bit endian-independent SWAR comparison, not unsafe unaligned `LQ`.
- MMI copy loads/stores are strictly interleaved to preserve the behavior
  of 16-byte-distance overlapping LZ77 history copies.

**Status:** source and build integration added; no PS2 hardware test or
R5900 cross-build has been run by the author of these commits.

## CMake cross-build

Use an EE-specific toolchain file. The file's name and CPU/ABI flags depend
on your installed PS2 SDK/toolchain:

```sh
cmake -S . -B build-ee \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/your/ee-toolchain.cmake \
  -DWITH_MMI=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DWITH_GTEST=OFF \
  -DBUILD_TESTING=ON \
  -DZLIB_COMPAT=ON

cmake --build build-ee --target zlib-ng test_mmi_slide_hash test_mmi_compare256 test_mmi_chunkset test_mmi_roundtrip

# Optional experimental checksum, built separately for A/B testing:
cmake -S . -B build-ee-adler \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/your/ee-toolchain.cmake \
  -DWITH_MMI=ON -DWITH_MMI_ADLER32=ON \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=ON -DWITH_GTEST=OFF
cmake --build build-ee-adler --target test_mmi_adler32 bench_mmi_adler32

# Optional 64-byte compare prefilter (benchmark on EE before adopting):
cmake -S . -B build-ee-compare64 \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/your/ee-toolchain.cmake \
  -DWITH_MMI=ON -DWITH_MMI_COMPARE64=ON \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=ON -DWITH_GTEST=OFF
cmake --build build-ee-compare64 --target test_mmi_compare256 bench_mmi_compare256
```

`WITH_MMI` is OFF by default. When enabled, CMake requires a working
assembler probe for `LQ`, `PSUBUH` and `SQ`, disables MSA, and selects
compile-time native dispatch rather than Linux HWCAP runtime detection.
If the probe fails, ensure your compiler is targeting the R5900, and for
cross compilation consider `-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`
when the compiler test cannot link without an SDK startup/runtime.

The four standard test programs (and optional Adler-32 test) are **EE executables**, not host
executables. Run them on actual PS2 hardware using your usual ELF loader.
Successful output is:

```text
MMI slide_hash: PASS
MMI compare256: PASS
MMI chunkmemset_safe: PASS
MMI roundtrip: PASS
# With WITH_MMI_ADLER32=ON:
MMI Adler-32: PASS
```

- `test_mmi_slide_hash` compares aligned and unaligned hash arrays with a
  scalar reference, verifies out-of-range sentinels, and checks the head-only
  update path separately.
- `test_mmi_compare256` checks differing source/destination alignments,
  all 256 mismatch indices, equal inputs, and final-byte boundaries.
- `test_mmi_chunkset` checks overlapping forward copies, short distances,
  backwards/forward sources, all sixteen alignment offsets, truncated
  output space, and sentinel bytes outside the copied span.
- `test_mmi_roundtrip` compresses and decompresses four kinds of input
  patterns at levels 1/6/9, including buffers much larger than 32KB.
  It detects incorrect decoded bytes, lengths, and error statuses.
- Optional `test_mmi_adler32` checks checksum and copy equivalence to
  `adler32_c` over many lengths, alignments, patterns and incremental chunks.

For performance testing, compare a normal `WITH_MMI=ON` build to an EE
baseline using `WITH_MMI=OFF` with otherwise identical build flags.
Aligned fast paths may be slower for some small inputs, so measure complete
inflate/deflate streams before drawing conclusions.

For ordinary builds that should not use MMI, omit `-DWITH_MMI=ON`.
For an EE baseline without MMI, build the same target with
`-DWITH_MMI=OFF -DWITH_MSA=OFF` and
`-DWITH_RUNTIME_CPU_DETECTION=OFF`.

## Autoconf-style configure script

If your EE toolchain can drive the project's existing configure script,
the experimental option is:

```sh
CC=/path/to/ee-gcc ./configure --with-mmi --static
make
```

The script also checks for MMI assembly instructions. It does not use
Linux MSA/HWCAP probing, and it disables runtime CPU detection for the
EE configuration. Platform-specific static-link and runtime settings
remain the responsibility of the PS2 toolchain integration.

## Suggested validation

1. First run the three `test_mmi_*` executables on hardware. Capture any
   assembler error, crash, or mismatch with its reported test parameters.
2. Run full compress/decompress round trips with levels 1, 6, and 9, with
   incompressible, repetitive, and near-32KB-window inputs. Compare decoded
   bytes against the unmodified zlib reference implementation.
3. Compare compressed output sizes and throughput against a no-MMI EE build.
   Measure whole-stream times, not just the vector loop.
4. Validate with both `-O2` and `-O3`. Confirm compressed streams decode
   correctly with an independent zlib implementation as well.

Please report toolchain version, compiler flags, MMI test output, and any
observed difference from the generic EE baseline.

## Important caveats

- This is a fixed R5900 target. It does **not** probe generic MIPS at runtime.
- The 128-bit MMI instructions operate on EE GPRs, not MIPS MSA registers.
- The fast paths rely on the compiler honouring GNU inline-assembly register
  clobbers for `$8` and `$9` and on the target toolchain's ABI.
- The tests are intended for execution on PS2, not QEMU's generic MIPS CPU.
- The experimental compare/copy fast paths have not yet been compiled with
  an R5900 toolchain or executed on an EE by the implementer. The available
  host Clang does not recognize `-march=r5900`; there is no EE cross-compiler
  installed in this environment.
- `BUILD_SHARED_LIBS=OFF` is recommended for PS2.

## Experimental Adler-32 throughput measurement

Build with `WITH_MMI_ADLER32=ON`, then execute `bench_mmi_adler32`
on a PS2. It compares native `adler32_mmi` with `adler32_c` at
64B, 1KB, 8KB and 64KB sizes, both aligned and offset-by-one.
The ratio is `C_ticks / MMI_ticks`; values above 1 suggest the MMI
candidate is faster. `clock()` on EE toolchains may have coarse or
unsupported timing: if it reports zero or unavailable ticks, use an
EE-specific hardware cycle counter instead. This executable is not
registered with CTest.

## Optional 64-byte comparison benchmark

Configure with `WITH_MMI=ON -DWITH_MMI_COMPARE64=ON` and build
`test_mmi_compare256` and `bench_mmi_compare256`.
Run the executable on EE hardware. The normal 256-byte regression test
checks every mismatch offset and all input alignment residues; the benchmark
also compares the optional 64-byte prefilter against generic C at different
match lengths and offsets. Its ratio is generic ticks divided by MMI ticks.
Both `WITH_MMI_COMPARE64` and `WITH_MMI_ADLER32` remain OFF by default.

## Architecture-independent Adler-32 math test

The weighted-sum helper is pure C, and its arithmetic is testable on a
normal workstation even if the EE cross compiler is not installed:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror \
  -o test_mmi_adler32_math test/test_mmi_adler32_math.c
./test_mmi_adler32_math
```

This test checks the scalar model of MMI's halfword pairs, modulo bounds,
input alignment and streaming split behavior. Passing it does **not**
validate the actual EE instructions; those still require PS2 execution.

## Experimental MMI Chorba CRC-32

Paper: Sam Russell, *Chorba: A novel CRC32 implementation*
(https://arxiv.org/abs/2412.16398), specifically the degree-44
`chorba_352` zero polynomial, scaled by 128 so that each XOR
offset falls on a 16-byte EE GPR boundary.

The chronological XOR taps (in bytes) are:
`16, 48, 112, 144, 192, 208, 448, 592, 624, 704`.
These are the exponents **in ascending order** multiplied by 16.
For reflected CRC-32, using `(44 - exponent) * 16` would be wrong.
The 704-byte zero-input reverse shift uses a precomputed 32-column
GF(2) matrix. The public CRC seed is XOR-folded into the first four
bytes, and the input buffer is never modified.

```sh
cmake -S . -B build-ee-chorba \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/your/ee-toolchain.cmake \
  -DWITH_MMI=ON -DWITH_MMI_CHORBA=ON -DWITH_CRC32_CHORBA=ON \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=ON -DWITH_GTEST=OFF
cmake --build build-ee-chorba --target zlib-ng test_mmi_chorba test_mmi_roundtrip
```

Run `test_mmi_chorba` on the PS2. Expected: `MMI Chorba: PASS`.
It compares native MMI against generic braid for different sizes,
input alignments, CRC seeds, repeated patterns, copy variants, and
streaming splits. Compare large-buffer throughput on actual EE
hardware before enabling this globally. The 1024-byte ring traffic
and 704-byte finalization can make small inputs slower than braid.
