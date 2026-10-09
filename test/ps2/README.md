# PS2SDK EE one-launch kernel tournament

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
# only build-ps2-mmi/zlib_ng_mmi.elf is generated
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

- `slide_hash`: serial / two-wide loads / four-wide loads
- `compare256`: generic / 16-byte or 32-byte or 64-byte prefilter, with
  bytewise or SWAR mismatch location
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
