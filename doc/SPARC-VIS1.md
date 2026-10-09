# Experimental SPARC VIS1 support

This branch adds **VIS1-accelerated `compare256`** using the 64-bit VIS
`fcmpeq8` instruction, plus VIS1-specialized `longest_match`, `longest_match_slow_knuth`,
and `longest_match_slow_roll` so actual DEFLATE match searches can call VIS1.
This branch also adds VIS1 Adler-32 and the Adler-32 copying variant using
`fpmerge` and `fpadd16` with exact, bounded 16-bit lane sums.
The experimental `slide_hash_vis1` and `slide_hash_head_vis1` now also use
VIS1 packed 16-bit instructions for the standard 32768-byte window.
Non-standard window sizes retain the scalar path. Generic fallbacks
remain available for all operations. The SPARC build now also has a conservative
64-bit FP load/store `chunkmemset_safe_sparc` variant for LZ77 copies,
using interleaved 8-byte copies at distances of at least eight bytes.
It is SPARC-specific and built under the VIS1 option, but its
`ldd`/`std` instructions are ordinary SPARC floating-point instructions,
not special VIS SIMD opcodes.

## Scope and safety

- `WITH_VIS1` defaults to OFF. Enable it only if you intend to test VIS1.
- Optional `WITH_VIS1_CHUNKSET=OFF` and `WITH_VIS1_SLIDEHASH=OFF`
  independently disable the unverified copy and packed slide-hash paths.
  Both sub-options default to ON when `WITH_VIS1` is enabled.
  Reconfigure in a fresh build directory when comparing variants.
- The compiler must accept `-mvis` and the inline assembly; CMake probes for both.
- On Linux/SPARC, runtime dispatch uses `getauxval(AT_HWCAP)` and Linux's
  SPARC VIS bit (0x00002000, bit 13). Only VIS-specific sources use `-mvis`.
- Without that runtime-detection API, CMake disables VIS1 for runtime builds.
  You can explicitly target VIS1-capable hardware with
  `-DWITH_RUNTIME_CPU_DETECTION=OFF`. Do not run such a build on pre-VIS CPUs.
- `compare256_vis1` uses `ldd` only when both pointers are 8-byte aligned,
  otherwise it compares bytes. At most 256 bytes are accessed.
- `adler32_vis1` peels misaligned bytes before 4-byte `ld` loads, processes
  exact 64-byte vector batches, and reduces modulo 65521 every 5552 bytes.
  VIS1 `fexpand` shifts bytes left 4 bits and **must not** be used as a
  zero-extending substitute for `fpmerge` in Adler-32.
- `adler32_copy_vis1` computes the checksum before copying source data.
- `slide_hash_vis1` uses `fcmpgt16` with zero to detect unsigned values
  at least 0x8000 via their signed sign bit, then applies `fpsub16`
  and `fand`. A 16-entry 64-bit mask lookup expands the four comparison
  bits into four full-word masks. The result implements
  `max(value - 32768, 0)` for all 65536 possible position values.
  Its 8-byte loads and stores are aligned by peeling scalar elements,
  with scalar handling for any remainder.
- `chunkmemset_safe_sparc` accelerates only copies with matching
  8-byte source/destination alignment, a backward LZ77 distance of at
  least eight bytes, and at least 32 requested/output-available bytes.
  Its interleaved 8-byte loads/stores preserve overlap semantics at
  distances 8, 16, and 24, while a scalar peel and tail respect the
  requested length exactly. All other cases call `chunkmemset_safe_c`.
  A direct CTest checks multiple lengths, distances, alignments and
  clipped `left` values against a bytewise reference.
- `HAVE_SPARC_VIS1_CHUNKSET_ASM` independently checks the `ldd`/`std`
  base+offset assembly used by the LZ77 copy path. If the probe fails,
  only that path is disabled; the generic copy remains available.
- The separate `HAVE_SPARC_VIS1_SLIDEHASH_ASM` compile probe controls
  inclusion of the slide hash optimization; other VIS1 operations remain
  available even if it fails.
- Since the packed implementation performs an indexed mask load per
  four entries, its *performance benefit has not been established*.
  Please benchmark before deciding whether it should be enabled by
  default.

## Linux SPARC64 cross-build

```sh
cmake -S . -B build-sparc-vis1 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-sparc64.cmake \
  -DWITH_VIS1=ON -DWITH_RUNTIME_CPU_DETECTION=ON \
  -DBUILD_TESTING=ON -DWITH_GTEST=OFF
cmake --build build-sparc-vis1 -j2
ctest --test-dir build-sparc-vis1 --output-on-failure
```

The provided toolchain file specifies `qemu-sparc64` for tests. Select
a VIS1-capable emulated CPU or use a real UltraSPARC system. Running on
a non-VIS CPU will exercise generic dispatch; the direct-VIS test skips.

For a **VIS1-only** build, including non-Linux platforms:

```sh
cmake -S . -B build-vis-only -DWITH_VIS1=ON \
  -DWITH_RUNTIME_CPU_DETECTION=OFF \
  -DBUILD_TESTING=ON -DWITH_GTEST=OFF
cmake --build build-vis-only -j2
ctest --test-dir build-vis-only --output-on-failure
```

The second command assumes a VIS1-capable target CPU and a compiler
that accepts VIS1 instructions.

## CI validation

The branch-specific workflow
[`SPARC VIS1 validation`](../.github/workflows/sparc-vis1.yml)
runs portable C99 arithmetic models with GCC and Clang and attempts SPARC64
cross-compilation and CTest under QEMU. Review logs for skipped VIS1 tests:
skipped tests have **not** executed VIS instructions. A successful emulator
run is not proof of correctness or performance on real UltraSPARC hardware.

## Portable correctness models

On any supported host (including x86), the `sparc_vis1_models` CTest
checks Linux/SPARC HWCAP decoding, Adler-32's four-lane reduction and all
16 packed slide-hash lane masks. It contains no SPARC assembly:

```sh
cmake -S . -B build-models -DBUILD_TESTING=ON -DWITH_GTEST=OFF
cmake --build build-models --target sparc_vis1_models -j2
ctest --test-dir build-models -R '^sparc_vis1_models$' --output-on-failure
```

These tests verify the arithmetic contract, **not** actual VIS instruction
encoding, inline assembler register allocation or CPU performance.

## Validation

1. Verify `HAVE_SPARC_VIS1_ASM=Success` and that VIS sources use `-mvis`.
2. Run `ctest --test-dir <build> -R 'vis1_' --output-on-failure`.
   `vis1_compare256` checks all 8 x 8 alignment pairs and all 257 mismatch
   positions (16,448 cases). `vis1_adler32` checks 8 alignments, 3 data
   patterns, 24 lengths (including 5552 and longer), and 4 seeds, plus
   byte-for-byte copy checks (2,304 cases).
   `vis1_slide_hash` covers all 16 packed lane masks, alignments,
   five input patterns, nonstandard window sizes and guard canaries
   (160 combinations).
   `vis1_chunkmemset` covers backwards LZ77 distances, alignments,
   varying `left` limits and overlapping output, compared against
   an independent bytewise oracle.
   On machines without VIS1, direct-ISA tests return 77 and CTest explicitly
   reports them as **Skipped**, not passed.
3. Run compression/decompression tests and compare against a separate
   `-DWITH_VIS1=OFF` build. Include checksum-heavy workloads.
4. Benchmark `compare256`, `longest_match`, `slide_hash`, LZ77 copy and
   Adler-32. Keep unproven paths opt-in until target measurements are available.

The implementation uses the project's `ALIGNED_` macro for C99 compatibility.

**Status:** SPARC compilation and real/emulated VIS1 execution remain
unverified until CI or target test results are available.
