# PS2 Linux MMI: one-transfer verification kit

This kit is designed to avoid rebuilding and retransferring programs every
time a candidate fails or is slower than its baseline. Build the full
experiment matrix on the development PC **before** visiting the PS2, move
the resulting directory to PS2 Linux once, run all programs, then collect
only their text logs.

It does **not** claim the supplied programs have already passed an R5900
cross-build, PS2 Linux execution, or hardware timing.

## 1. Build all variants on the development PC

You need a working PS2 Linux R5900 CMake toolchain and C runtime. The
toolchain path must point at an actual `.cmake` file; the build script does
not install an SDK, choose a compiler for you, or use the internet.

```sh
sh test/ee-build-matrix.sh \
  /absolute/path/to/ee-toolchain.cmake \
  /absolute/path/to/ps2-ee-mmi-bundle
```

The build stops immediately if any configuration cannot compile or link.
Do **not** transfer an incomplete bundle to the PS2. Inspect the first
compiler/assembler/linker error on the PC and fix it before the trip.

The resulting directory has `manifest.csv`, ten `mmi_suite_*` EE ELF
executables, and `run-all.sh`. The ten builds consist of one control,
eight isolated feature changes, and one combined configuration:

| Configuration | Single runtime dispatch change relative to control |
| --- | --- |
| `baseline` | All experimental candidate flags OFF |
| `compare_swar` | SWAR mismatch location |
| `slide_interleaved` | Interleaved slide_hash loads |
| `chunkset_burst` | Safe large-distance LZ77 load burst |
| `chunkset_pattern` | Periodic distance-1/2/4/8 MMI reconstruction |
| `adler_formula` | Unrolled Adler weighted sum |
| `adler_fused_copy` | Fused Adler checksum and copy |
| `chorba_paired` | Paired XOR scatter taps |
| `chorba_fused_copy` | Fused CRC and copy |
| `combined` | All eight candidates enabled together |

All ten builds have the parent MMI, 64-byte compare prefilter, experimental
Adler-32, and experimental Chorba CRC-32 enabled. Hence `baseline` is a
**control among MMI builds**, not generic zlib without MMI. Each suite also
benchmarks generic C and several original kernels directly.

## 2. One copy to PS2 Linux, all runtime tests

Copy the entire bundle to a filesystem from which PS2 Linux can execute
EE ELF programs. On the PS2:

```sh
cd /path/to/ps2-ee-mmi-bundle

# Short, early safety gate across all configurations:
sh run-all.sh --smoke

# All correctness tests, including deterministic randomized stress:
sh run-all.sh --tests

# Benchmarks on the same input/feature combinations:
sh run-all.sh --benches

# Or do correctness and benchmarks in one command:
sh run-all.sh --all
```

`run-all.sh` returns nonzero if any variant fails. It continues running
the other variants and writes each program's stdout/stderr separately under
`results/`, so one failure does not erase the other measurements.

The `mmi_suite` executable also supports:

```sh
./mmi_suite_baseline --list
./mmi_suite_baseline --only test_stress
./mmi_suite_baseline --only bench_roundtrip
./mmi_suite_baseline --tests --fail-fast
```

Each executable writes machine-readable lines such as:

```text
MMI_SUITE_FEATURE,chunkset_pattern,OFF
MMI_SUITE_BEGIN,test,test_chunkset
MMI_SUITE_RESULT,test,test_chunkset,PASS,0,12345
MMI_SUITE_SUMMARY,8,8,0,7
```

Numbers are illustrative, **not** measured PS2 results. The individual
kernel tests and microbenchmarks also print their normal explanatory
output within the same log.

## 3. Bring the result logs back, not the binaries

Copy `results/` back to the development PC, keeping it next to
`manifest.csv`. Run:

```sh
python3 test/ee-summarize-results.py /path/to/ps2-ee-mmi-bundle
```

The standard-library-only script writes `comparison.csv` with per-pattern
and per-level compressed sizes, compression/decompression ticks, and ratios
to the baseline. A ratio greater than 1 means the isolated candidate was
faster in that run. Inspect `MMI_SUITE_RESULT` before trusting any timing.

Run at least twice if possible. PS2 Linux `clock()` may be too coarse or
behave differently from a hardware cycle counter; `clock_unavailable_or_too_coarse`
rows are not speed measurements. A large speed gain in a microkernel does
not automatically yield a win in the full deflate/inflate stream.

## 4. How to diagnose problems without rebuilding everything

- An ELF does not load at all: check the PS2 Linux executable format, ABI,
  libc, and linker settings provided by the R5900 toolchain.
- One test fails: use `--only test_<name>` on the *already transferred*
  suite executable, and copy its failure line (case and seed).
- One feature is slower: compare the matching entries in
  `comparison.csv`; the original implementation is still included in
  every standalone A/B kernel test.
- All tests pass, speed is unclear: inspect `CLOCKS_PER_SEC` and
  `clock_unavailable_or_too_coarse`, then collect cycle counters on
  real PS2 hardware rather than using an emulator for definitive timing.

The host CI checks the generic build, arithmetic reference models,
Python script syntax, and shell syntax. **It is not R5900 instruction,
PS2 Linux ABI, or hardware timing verification.**

## 5. Features intentionally not rewritten

The generic Huffman bit decoder and `inflate_fast` state machine remain
unchanged, aside from dispatching to the MMI history-copy kernel.
Those functions are highly branch-dependent and cannot be replaced safely
with an arbitrary LQ/SQ loop. This kit fully exercises its modified match,
hash, copy and checksum paths, but does not claim to optimize every line
of the zlib-ng library.
