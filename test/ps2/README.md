# PS2SDK EE one-launch kernel tournament

## Expanded one-launch scratchpad experiment matrix

The default QUICK mode includes all SPR correctness checks and timing
with reduced repetition counts. Use the QUICK run first to avoid wasting
real-hardware time; reserve --full for a confirmed stable build.
A single final GS screen now reports representative RAM/SPR ratios for
Chorba, Compare256, and Adler-32. Detailed input sizes are stdout-only.



Chorba now has nine ranked candidates: the original six, spr_ring,
spr_residue and spr_both. These isolate the 1024-byte scatter ring
and 704-byte final residue in RAM or EE SPR, with the same CRC check.
Adler-32 additionally tests genuine prefix/formula MMI checksum kernels
on RAM, preloaded SPR, and copy-inclusive SPR for 64/1024/8192/
16368-byte inputs, aligned and offset by one byte. Output records:
ZLIB_SPR_ADLER and ZLIB_SPR_ADLER_RESULT.
Compare256 also benchmarks genuine plain and prefilter64_swar MMI
with RAM, first input in SPR, second in SPR, both, and copies inside
the timed loop. Three alignments and six mismatch locations are covered.
The additional stdout records are ZLIB_SPR_COMPARE and
ZLIB_SPR_COMPARE_RESULT. No production dispatch changes.

bash test/ps2/build.sh now builds ONLY build-ps2-mmi/zlib_ng_mmi.elf;
it does not build ps2_mmi_test and removes stale
build-ps2-mmi/zlib_ng_mmi_test_only.elf. The CMake target remains
available for manual explicit builds, but is not part of this script.


## PS2 scratchpad CRC32 Chorba benchmark

PS2 mmi_suite adds a seventh chorba competitor named spr_ring.
It uses paired ten-tap EE MMI XOR scatter with the 1,024-byte ring
at 0x70000000 in the EE 16 KiB scratchpad instead of on the stack.
All sizes and offsets are validated against crc32_braid before timing.
Short sizes may use braid fallback, so look at the large-size results.
Timing includes ring clearing and residue reduction; it excludes DMA
(there is no DMA). It is benchmark-only and not production dispatch.

Build as usual: bash test/ps2/build.sh
Run zlib_ng_mmi.elf, preferably using --full for the detailed test.
Capture MMI_CANDIDATE,chorba and MMI_WINNER,chorba records.
The run must have exclusive SPR ownership and no concurrent SPR/DMA
user. Neither the cross-build nor hardware timing has been verified.


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

The eight kernel competitions are:

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
three independent tests (including exhaustive compare256), plus all eight
benchmark entrypoints. The standalone `--all` mode runs sixteen entries.
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
