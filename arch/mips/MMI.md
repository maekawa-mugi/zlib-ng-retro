# PlayStation 2 Emotion Engine (R5900) MMI

Optimized kernels for the PS2 EE, built on the R5900 128-bit multimedia
instructions (`LQ`, `SQ`, `QFSRV`, `PXOR`, `PEXT*`, `PADDH`, ...) and a few
hand-scheduled assembly loops. This is **not** MIPS MSA and must never be
enabled for generic MIPS CPUs.

## Building

`WITH_MMI=ON` is the only switch needed; every kernel that measured faster
than generic C on a real PS2 is then used. An EE cross toolchain is required
(`cmake/toolchain-ps2-ee.cmake` works with PS2DEV/PS2SDK):

```sh
cmake -S . -B build-ee \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-ps2-ee.cmake \
  -DCMAKE_BUILD_TYPE=Release -DWITH_MMI=ON \
  -DBUILD_SHARED_LIBS=OFF -DWITH_GTEST=OFF -DWITH_GZFILEOP=OFF
cmake --build build-ee
```

Runtime CPU detection is disabled in this configuration (the EE is a fixed
target; MSA's HWCAP probe cannot detect MMI).

## What is used

Measured on a real PS2, generic C = 1.00x (mixed-size/alignment benchmarks):

| Function | Implementation | Speed-up |
|---|---|---|
| `compare256`, `longest_match*` | `compare256_mmi_qf.c`: `LQ` + `QFSRV` realignment for skewed pointers | ~1.9x |
| `crc32`, `crc32_copy` | `crc32_gather_mmi.c`: zero-polynomial CRC in "gather" form (below) | ~1.75x (copy ~2.3x) |
| `adler32`, `adler32_copy` | `adler32_mmi_vec.h`: multiplier-free halfword prefix sums | ~1.2x (copy ~2.2x) |
| `chunkmemset_safe` (LZ77 copy) | `chunkset_mmi_fast.c` | ~1.5-1.75x |
| `slide_hash` | `slide_hash_mmi.c`: 4-load schedule | ~5.6x |
| `inflate_fast` | `inflate_fast_mmi.c` + `inflate_fast_asm.h`: hand-scheduled decode loop (64-bit unaligned-load bit buffer, 6-op refill, register-resident masks), MMI-copy fallback via `chunkmemset_safe_mmi_fast` | emulator: ~2-3% over the compiled loop |
| bulk copies | `memcpy_mmi.c` (`zng_mmi_memcpy`): `LQ`/`SQ` + `QFSRV`; used for window slides, stored blocks, pending output and input reads via `ZNG_BULK_MEMCPY` | ~3.7x vs libc |

Inflate/deflate end-to-end throughput on the same machine is 100-130 MB/s
for compressible data and ~30 MB/s for incompressible data at 262144 bytes.

## CMake options

All production options default to ON when `WITH_MMI=ON` and can be switched
off individually to fall back to generic C:

`WITH_MMI_COMPARE256_DISPATCH`, `WITH_MMI_ADLER32` / `_DISPATCH` /
`_COPY_DISPATCH`, `WITH_MMI_CHORBA` (the CRC-32 kernels; needs
`WITH_CRC32_CHORBA=ON`) / `_DISPATCH` / `_COPY_DISPATCH`,
`WITH_MMI_BULK_MEMCPY`, `WITH_MMI_INFLATE_FAST` (MMI-copy `inflate_fast`) and
`WITH_MMI_INFLATE_FAST_ASM` (the assembly loop; turn it off to use the
compiled loop).

Experimental alternatives (all OFF, kept for A/B work, none of them is
faster than the production choice above): `WITH_MMI_COMPARE64`,
`WITH_MMI_COMPARE32`, `WITH_MMI_COMPARE_SWAR`, `WITH_MMI_ADLER32_FORMULA`,
`WITH_MMI_ADLER32_FUSED_COPY`, `WITH_MMI_SLIDE_HASH_INTERLEAVED[2]`,
`WITH_MMI_CHUNKSET_BURST`, `WITH_MMI_CHUNKSET_PATTERN[128]`,
`WITH_MMI_CHORBA_PAIRED_TAPS`, `WITH_MMI_CHORBA_FUSED_COPY`,
`WITH_MMI_CHORBA_THRESHOLD`.

## Notes on individual kernels

### CRC-32 "gather" form

Chorba's scatter form updates `T` future ring slots per 16-byte block (load,
xor, store each). Evaluating the same recurrence by gathering,
`v[i] = in[i] ^ v[i-t1] ^ ... ^ v[i-tT]`, costs one load per tap, `T` xors and
a single store, with no slot clearing and no per-tap address masking: read
pointers are computed once per run and a run never crosses a ring wrap
(`crc32_gather_mmi.h`, one kernel per tap count). The ring starts `max-tap`
bytes in so no pointer wraps at the start. The look-ahead the stream still
owes is rebuilt from the ring with the same kernel, checksummed, and mapped
back through the inverse of `x^(16*degree*8)`.

Zero polynomials (reflected generator `0x1DB710641`, exponents in 16-byte
units; each is the unique polynomial of its term count at the minimal degree
published in the Chorba paper):

| terms | degree | exponents |
|---|---|---|
| 11 | 44 | 44 39 37 28 13 12 9 7 3 1 0 |
| 9 | 66 | 66 64 63 48 47 34 29 9 0 |
| 5 | 300 | 300 211 183 145 0 |

Degree 44 is used for short buffers, 66 up to ~20 KiB and 300 above.
Buffers shorter than the crossover use the generic braid CRC.

### Adler-32

Buffers under 32 bytes use the generic code. Longer buffers are processed in
windows of at most 346 quadwords so the exact sums stay below 2^32;
head and tail bytes are masked rather than handled separately.

### LZ77 copy

`chunkset_mmi_fast.c` implements the `chunkmemset_safe` contract, including
overlapping copies (`dist < len`), where every byte must be read after it was
written.

### Bulk copy

`zng_mmi_memcpy` behaves like `memcpy` (non-overlapping) and handles
arbitrary source/destination alignment.
