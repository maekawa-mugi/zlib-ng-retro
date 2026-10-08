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
- `chunkmemset_safe_mmi`: MMI 128-bit LZ77 history copying for aligned
  source/destination addresses and distance >= 16; generic C fallback for
  short-distance, differently aligned, or backward-overlapping copies.
- Experimental `adler32_mmi` and `adler32_copy_mmi` are available with
  `WITH_MMI_ADLER32=ON` (OFF by default). They combine MMI byte-to-halfword
  expansion and packed addition with exact scalar weighted sums, without
  PMADDH or implicit HI/LO changes. Benchmark before enabling in production.
- CRC-32 still uses generic implementations. The generic
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
cmake --build build-ee-adler --target test_mmi_adler32
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
