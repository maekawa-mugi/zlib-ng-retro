# PS2SDK EE two-ELF original zlib vs zlib-ng MMI comparison

## Original zlib 1.3.2 vs zlib-ng MMI: identical end-to-end harness

`bash test/ps2/build.sh` now produces **TWO standalone EE binaries**,
without symbol interposition or mixing implementations:

- `build-ps2-mmi/zlib_ng_mmi.elf`: existing complete 17-suite R5900
  kernel validation/tournament and 36-case zlib-ng MMI roundtrip.
- `build-ps2-mmi/zlib_original.elf`: unmodified official zlib **1.3.2**
  compressor/decompressor with exactly the same roundtrip benchmark source,
  36 pattern/size/level conditions, six rotating-order samples, median and
  original-byte MB/s calculation, and a distinct on-screen title.

The 21 required **pristine original zlib source files** are vendored in
`third_party/zlib-1.3.2/` along with the original LICENSE. Their Git
blob SHA-1 values match both the user's `zlib-1.3.2.tar.gz` and the
official upstream `madler/zlib` `v1.3.2` tag. This is the build-core
subset of the original distribution, not every upstream example/build file.
The vendored source has not been modified.

Both ELFs print `RT_META`, `RT_CASE`, and `RT_RESULT` records with
an explicit implementation field (`zlib-1.3.2` or `zlib-ng-mmi`).
**Both implementations** must pass input-byte-for-byte verification
before reporting their rate. The original ELF shows the current
`C xx.xx MB/s | D yy.yy MB/s` on its own GS screen.

Build and copy, from the repository root:

```sh
bash test/ps2/build.sh
mkdir -p "$HOME/elfs"
cp -f build-ps2-mmi/zlib_ng_mmi.elf "$HOME/elfs/"
cp -f build-ps2-mmi/zlib_original.elf "$HOME/elfs/"
```

Capture each ELF's **complete stdout** to a separate file, and compare
the *same* 36 cases on host:

```sh
python3 test/ps2/compare_roundtrip.py zlib_original.log zlib_ng_mmi.log
```

Printed C and D ratios are `zlib-ng MMI MB/s / original zlib MB/s`.
Ratios over 1.0 mean zlib-ng's end-to-end operation runs faster.
The comparison script checks all 36 cases, correct implementation labels,
matching repeat counts and valid clock values; mismatched cases are errors.

**Fairness/corrections:** The original and zlib-ng **compress** their
same source bytes independently. Their compressed sizes may differ, so
the **decompression** figures do not by themselves prove that one
inflate engine is faster *on an identical compressed stream*. Those
numbers measure each codec's own full roundtrip. This is a two-way
**original-zlib vs zlib-ng-MMI** comparison, NOT a three-way scalar
zlib-ng baseline. A future decoder-isolation experiment should use
a single precompressed fixture in both ELFs. The GS's mixed kernel
winner cannot substitute for whole-stream throughput.

The `slide_hash` load4 microkernel won **5.61x** on the supplied real
PS2 log, but `slide_interleaved=OFF` in that build, so do not credit
the 5.61x to whole-stream zlib-ng. The scalar/serial/load2/load4
microkernel contest remains available separately. `crc32_braid` and
`table256` are **non-Chorba** reference methods; `small300` and
`dense4_5869` are experimental Chorba-family polynomials with
size-dependent wins, *not* default runtime routes. As with any kernel
microbenchmark, include allocation and workspace costs before using
a speed claim for production.

Host regression (no PS2SDK needed):

```sh
sh test/ps2/test_stock_zlib_host.sh
python3 test/ps2/test_compare_roundtrip.py
sh test/ps2/test_rank_host.sh
```

---


## October 2026: fairer R5900 A/B harness (branch ps2-ee-mmi)

The on-screen table's **MIXED BEST** is the candidate with the lowest
summed elapsed time over the runner's specified workload mix. This is
**not** a general-purpose, per-input winner and is **not** a whole-stream
zlib-versus-zlib-ng comparison. Each comparable benchmark condition now
also emits `MMI_CASE_WINNER,<family>,<case_number>,<candidate>,<C_over_candidate>,<C_ticks>,<candidate_ticks>`.
Invalid clocks and correctness failures are `UNDETERMINED`. Full raw
per-variant timings remain on stdout. Compare cases by their existing
length, alignment, mismatch and distance fields.

`slide_hash` now benchmarks **four** candidates: independent scalar C
(reference column zero), serial MMI, MMI two-load and MMI four-load.
Its hash-table restoration is performed *outside* the measured function
call; the current timer still incurs per-call `clock()` overhead, so
small differences remain exploratory.

`compare256` now benchmarks **nine** checked candidates: the former
seven plus `hybrid16` and `hybrid16-64`. The two additional candidates
compare the initial 16 bytes with safe 64-bit SWAR XOR before paying for
MMI LQ/PXOR/SQ and optional 64-byte prefilter work. They are validated
across every 0..256 mismatch index and all 16x16 alignment combinations
by `test_mmi_compare256`. Neither changes production dispatch.

The independent `roundtrip` phases are NOT ranked against one another.
Each of 36 combinations (four patterns × 4/64/256 KiB × levels 1/6/9)
now runs **six** alternating compression/decompression sample batches,
reports a median elapsed tick count per phase and measures throughput
using the **original byte count for both**:

`MB/s = source_bytes * repetitions * CLOCKS_PER_SEC / median_ticks / 1000000`

Each validated case emits `RT_CASE,zlib-ng-mmi-build,pattern,level,bytes,compressed_bytes,reps,compress_median_ticks,decode_median_ticks,compress_MBps,decode_MBps`.
`RT_INVALID` explicitly marks invalid or coarse timer readings rather
than inventing a throughput. The GS row 16 displays the latest
`C MB/s` (compression) and `D MB/s` (decompression), in white with
green restricted to the word PASS. The implementation label
`zlib-ng-mmi-build` means the MMI-capable library was linked, **not**
that every function took its MMI route.

This is now a two-way original-zlib/zlib-ng-MMI end-to-end A/B
harness with separate ELFs. A scalar zlib-ng configuration remains
an independent future baseline. To compare absolute PS2 performance across
binaries, capture the `RT_CASE` CSV from identically configured scalar
and MMI builds; a stock-zlib build must use the same pattern generator,
level, input length, byte numerator and timer resolution. Cross-ELF
decode comparisons should also use a common compressed stream when
isolating pure decoder differences.

Host numerical/rank tests:

```sh
sh test/ps2/test_rank_host.sh
```

All accelerated candidates are experimental until a real R5900 run
shows correctness and a repeatable speed advantage.

## CRC32 zero-polynomial research (RAM-only)

This branch keeps the integrated zlib_ng_mmi.elf harness and removes **all
PS2 scratchpad benchmarks**, scratchpad selection and scratchpad display.
The product CRC32 dispatcher remains unchanged. Only the standalone PS2
benchmark ELF links the new experiment in `test/ps2/crc_poly_mmi.c`.

The CRC32 experiment is based on Sam Russell's *Chorba: A novel CRC32
implementation* (arXiv:2412.16398). Each candidate's polynomial is
verified modulo the **reflected** CRC32 generator 0x1DB710641. The
original paper's dense degree-5869 four-term expression must be
reciprocated to x^5869 + x^48 + x^34 + 1 for this representation.
All zero-polynomial exponents are scaled by 128 to match the
R5900's 16-byte LQ/PXOR/SQ operations. The 32-column inverse CRC
transform is separately derived for each residue length.

`bench_crc_poly` tests 13 variants: normal CRC32 braid, a 256-byte
lookup table, bitwise reference for short buffers, original Chorba
single/paired, and eight new polynomial schedules:
`gen32`, `chorba352`, `small300`, `small600`, `sparse4_3006`,
`dense4_5869`, `dense5_14870`, `sparse3_91639`. Large-degree
schemes are tried only at sizes where their 128-bit look-ahead fits;
ineligible candidates are explicitly marked `SKIP_BELOW_MINIMUM`,
**never** credited with a fallback win.

Input lengths span 128 B, 1 KiB, 4 KiB, 8 KiB, 16 KiB, 32 KiB,
64 KiB, 256 KiB, 1 MiB and 4 MiB, at offsets 0/1. Every eligible
variant must equal braid CRC32 on the exact same input and initial CRC
before it is timed. For each candidate and size, six rotated-order
samples are taken. The reported time is the median per-call ticks,
not the aggregate over incompatible sizes. The existing nine GS
ranking groups are retained. Rows 19 and 22 show the separate
size-specific winners for 4 KiB, 64 KiB and 1 MiB.

Machine-readable records:
`CRC_POLY_META`, `CRC_POLY_SPEC`, `CRC_POLY_SAMPLE`,
`CRC_POLY_CASE`, `CRC_POLY_WINNER`, `CRC_POLY_RESULT`.
The final `MMI_SUITE_SUMMARY` and `PS2 EE MMI RESULT` are still emitted.

Build (PS2SDK already configured):

```sh
bash test/ps2/build.sh
# build-ps2-mmi/zlib_ng_mmi.elf AND zlib_original.elf are generated
```

`QUICK` is the default; `--full` increases repetitions. Neither
mode selects a faster polynomial for production zlib-ng.
For host regression of every candidate, seed and buffer boundary:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -DCRC_POLY_HOST_TEST \
  -o /tmp/test_crc_poly_host \
  test/ps2/crc_poly_mmi.c test/ps2/test_crc_poly_host.c
/tmp/test_crc_poly_host
```

The R5900-only LQ/PXOR/SQ path still requires PS2SDK cross-compilation
and actual EE/PCSX2 execution before accepting hardware timing results.
The degree-91639 variant uses a 2 MiB ring in benchmark BSS and is
intentionally excluded from short input sizes.

**No ELF arguments are required.** Running `mmi_suite.elf` selects the
nonduplicated integrated validation, executes all enabled A/B/C kernels,
prints a verified fastest candidate per *comparable workload* and keeps the
results on the screen. It does **not** automatically rewrite the production
zlib dispatcher, and one quick run cannot rule out timing noise.

The default includes deterministic stress, exhaustive 16x16x257 compare256
alignment/mismatch checks, standalone Adler arithmetic checking and in-benchmark
correctness (once per test case, outside timing). It skips repeated standalone
hash, chunkset, roundtrip, Adler and Chorba tests whose cases are checked in
the benchmarks or stress, avoiding duplicate execution. Use `--all` when you
want every regression and every benchmark regardless of overlap. The screen
shows selection coverage and `MMI_SUITE_COVERAGE` records it in stdout.

The nine kernel competitions are:

- `slide_hash`: scalar C / serial / two-wide loads / four-wide loads
- `compare256`: generic / 16-byte or 32-byte or 64-byte prefilter, with
  bytewise or SWAR mismatch location, plus SWAR-first hybrid16/64
- `lz77_short`: generic / serial / 64-byte periodic store / 128-byte periodic store,
  only for distances 1, 2, 4, 8
- `lz77_long`: generic / serial / four-wide load/store,
  only for distances at least 64
- `adler32`: generic / prefix / weighted-formula
- `adler32_copy`: generic C+copy / MMI+copy / fused MMI
- `chorba`: braid / single tap / paired tap / thresholds 1024, 4096, 8192
- `chorba_copy`: braid C+copy / MMI+copy / fused MMI
- `crc_poly`: standalone 13-way CRC32 zero-polynomial comparison,
  reported by input size in `CRC_POLY_*` records, not pooled into one winner

Whole-stream `roundtrip` validates compression and decompression with full
output comparison, **not** against each other as competing candidates.

Each candidate is validated against a scalar reference *before timing*.
Measurements share the same case set and operation count. A candidate with a
mismatch, invalid timer, or a different measured case count cannot be declared
a winner. The summary is printed as:

```text
MMI_CANDIDATE,compare256,generic,36,0,36,0,123456
MMI_WINNER,compare256,64-swar,1.234
```

These numbers are examples, not PS2 timings. `MMI_WINNER,...,UNDETERMINED`
means the runner cannot safely rank that kernel. Each group prints all checked
candidate rows and the complete per-case results remain on stdout. Screen
comparisons are **not** automatically proof of performance improvements in
real workloads; vary input data and repeat with `--full` before choosing a
production build flag.

For the winning implementation, a later production build can set the
corresponding option. New switches are `WITH_MMI_COMPARE32=ON` (requires
`WITH_MMI_COMPARE64=ON`), `WITH_MMI_SLIDE_HASH_INTERLEAVED2=ON`, and
`WITH_MMI_CHUNKSET_PATTERN128=ON` (requires
`WITH_MMI_CHUNKSET_PATTERN=ON`). The normal test ELF always links every
variant, independent of the selected production dispatcher.

Production selection is now **independent of candidate compilation**.
`WITH_MMI=ON` keeps the tested serial MMI long-distance (>=64-byte) LZ77
copy path, but routes short and unmeasured distances through generic C.
The quick EE measurements favored C for compare256, Adler32 and CRC32, so
their production selection defaults to generic even with MMI kernels built.
After full-stream measurements, explicit opt-ins are
`WITH_MMI_COMPARE256_DISPATCH=ON` (includes longest_match),
`WITH_MMI_ADLER32_DISPATCH=ON`, `WITH_MMI_ADLER32_COPY_DISPATCH=ON`,
`WITH_MMI_CHORBA_DISPATCH=ON`, and `WITH_MMI_CHORBA_COPY_DISPATCH=ON`.
For fused production copy also set the matching `_FUSED_COPY=ON` flag.
The new three-way copy benchmark tests the scalar+memcpy baseline; older
1.52x/1.44x fused wins were only against MMI+memcpy and are not proof
of a win over C+memcpy.

The EE Core User Manual's latency section describes loads as 1-cycle under
cache hits but warns that immediate load-use dependencies can interlock, and
that dual issue, cache misses and pipeline hazards make instruction timing
non-additive. Therefore the new 2-way/4-way load orders, the 32B/64B
comparison prefilters and 64B/128B periodic SQ stores are **alternatives to
measure**, not a prediction of which schedule wins.

## Build

Use a modern `mips64r5900el-ps2-elf-gcc` toolchain, PS2SDK, CMake and make.
Run in MSYS2 MinGW32, Linux or WSL from the repository root:

```sh
export PS2DEV=/usr/local/ps2dev
export PS2SDK="$PS2DEV/ps2sdk"
bash test/ps2/build.sh build-ps2-mmi-all -G "Unix Makefiles" \
  # Parent MMI, 64B prefilter, Adler and Chorba are on by default here.
  # Candidate production dispatch remains OFF, but every candidate is linked.
```

In MSYS2, ensure the CMake executable and PS2DEV compiler are on `PATH`.
When using native Windows CMake, pass
`-DCMAKE_MAKE_PROGRAM=C:/msys64/usr/bin/make.exe` if necessary.
The toolchain recognizes Windows `.exe` compiler names. For split installs,
`PS2EE_CRT_DIR` can specify the directory containing `crt0.o`.

Both ELFs and `libz-ng.a` are written to the output directory. Copy the
chosen ELF to PS2 or open it with PCSX2 and your configured BIOS.

## Execution options

- No arguments: nonduplicated integrated correctness and ranked quick benchmarks.
- `--integrated --quick`: abbreviated validation for a fast preliminary screen.
- `--integrated --full`: abbreviated validation, original timing counts.
- `--all --full`: all standalone tests plus all benchmarks and original counts.
- `--tests`: standalone tests only.
- `--benches`: benchmark validation and timing only.
- `--only test_stress`: deterministic randomized stress only.
- `--list`: list entries on stdout.

With Adler, compare64 and Chorba enabled, default integrated mode runs
three independent tests (including exhaustive compare256), plus all nine
benchmark entrypoints. The standalone `--all` mode runs seventeen entries.
The display never labels incomplete or invalid measurements as fastest.

## Validation and fixes

The build was linked with MSYS2 MinGW32 GCC 15.2.0 and PS2SDK. ELF flags
and MMI instructions were inspected. PCSX2 screenshots showed successful
A/B benchmark checks; the latest roundtrip capacity fix still needs a
PCSX2 or hardware rerun. No real hardware timing is claimed.

Host checks verified display colors, live updates, time conversion, invalid
clocks, incomplete results, iteration counts and suite entry selection.
The generic stress ran 5,000 comparisons and 3,000 history copies. Injected
output corruption and violations of both output boundaries were detected.

The stress oracle permits writes in unused output capacity between the
requested length and `left`, as the generic chunk copier can write a full
chunk for a short copy. Requested output, return pointers, history and bytes
outside the allowed output capacity remain checked.

Roundtrip tests and benchmarks allocate output using `compressBound()`:
level 1 can expand random input by more than the previous 1KB allowance.
The old benchmark reproduced `Z_BUF_ERROR` for 256KiB random input at level
1 on the host; all 36 benchmark conditions passed after the capacity fix.
Allocation and release occur outside timed loops.
