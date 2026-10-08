# PlayStation 2 Emotion Engine MMI (experimental)

This branch adds opt-in R5900 MMI acceleration to **deflate's hash-table
sliding**, using the 128-bit `LQ`, `PSUBUH`, and `SQ` instructions. It is
not MIPS MSA, and must never be enabled for generic MIPS CPUs.

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
- Adler-32, CRC-32, and inflate copying still use generic implementations.

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

cmake --build build-ee --target zlib-ng test_mmi_slide_hash test_mmi_compare256
```

`WITH_MMI` is OFF by default. When enabled, CMake requires a working
assembler probe for `LQ`, `PSUBUH` and `SQ`, disables MSA, and selects
compile-time native dispatch rather than Linux HWCAP runtime detection.
If the probe fails, ensure your compiler is targeting the R5900, and for
cross compilation consider `-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`
when the compiler test cannot link without an SDK startup/runtime.

The generated `test_mmi_slide_hash` is an **EE executable**, not a host
executable. Run it on actual PS2 hardware using your usual ELF loader. When
the test succeeds, it prints:

```text
MMI slide_hash: PASS
```

The C test compares the MMI code to an independent scalar reference with
different window sizes, aligned and deliberately unaligned hash arrays, and
checks sentinel entries outside the arrays. It also verifies that the
head-only variant leaves the `prev` table untouched.

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

1. First run `test_mmi_slide_hash` on hardware. Capture any assembler error,
   crash, or mismatch including `wsize` and `offset`.
2. Run full compress/decompress round trips with levels 1, 6, and 9, with
   incompressible, repetitive, and near-32KB-window inputs. Compare decoded
   bytes against the unmodified zlib reference implementation.
3. Compare compressed output sizes and throughput against a no-MMI EE build.
   Measure whole-stream times, not just the vector loop.
4. Verify `slide_hash_head_mmi` and `slide_hash_mmi` independently in the
   diff test, ideally with `-O2` and `-O3` compiler settings.

Please report toolchain version, compiler flags, MMI test output, and any
observed difference from the generic EE baseline.

## Important caveats

- This is a fixed R5900 target. It does **not** probe generic MIPS at runtime.
- The 128-bit MMI instructions operate on EE GPRs, not MIPS MSA registers.
- The fast path relies on the compiler honouring the GNU inline assembly
  register clobbers for `$8` and `$9` and on the target toolchain's ABI.
- The test is intended for execution on PS2, not QEMU's generic MIPS CPU.
- `BUILD_SHARED_LIBS=OFF` is recommended for PS2.
