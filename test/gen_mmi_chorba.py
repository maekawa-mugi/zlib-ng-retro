#!/usr/bin/env python3
"""Regenerate/verify the EE MMI Chorba polynomial and inverse-CRC matrix.

Standard-library only: python3 test/gen_mmi_chorba.py

This validates the *mathematics*, NOT the PlayStation 2 MMI instructions.
Paper: https://arxiv.org/abs/2412.16398 (chorba_352).
"""
import random
import zlib

CRC32_REFLECTED_GENERATOR = 0x1DB710641  # reciprocal of 0x104C11DB7
CRC32_REVERSE_STEP_POLY = 0xEDB88320
EXPONENTS = (44, 39, 37, 28, 13, 12, 9, 7, 3, 1, 0)
TAPS = tuple(sorted(e * 16 for e in EXPONENTS if e != 0))
RESIDUE = 44 * 16
RING_MASK = 1024 - 1
U32 = 0xFFFFFFFF


def remainder(poly, generator):
    while poly.bit_length() >= generator.bit_length():
        poly ^= generator << (poly.bit_length() - generator.bit_length())
    return poly


def reverse_zero_bytes(crc, count):
    for _ in range(count):
        for _ in range(8):
            top = crc >> 31
            crc = (((crc ^ (CRC32_REVERSE_STEP_POLY if top else 0)) << 1) | top) & U32
    return crc


INVERSE = tuple(reverse_zero_bytes(1 << bit, RESIDUE) for bit in range(32))


def reverse_with_matrix(crc):
    result = 0
    for bit in range(32):
        if crc & (1 << bit):
            result ^= INVERSE[bit]
    return result


def chorba_model(crc, data, alignment=0, threshold=4096):
    """Pure-Python, 128-bit-lane-equivalent emulation of the MMI Chorba loop."""
    peel = (-alignment) & 15
    if len(data) < threshold + peel:
        return zlib.crc32(data, crc)
    crc = zlib.crc32(data[:peel], crc)
    data = data[peel:]
    n = len(data) & ~15
    ring = [0] * (1024 // 16)
    for i in range(0, n, 16):
        index = (i >> 4) & 63
        value = int.from_bytes(data[i:i + 16], "little") ^ ring[index]
        if i == 0:
            value ^= (~crc) & U32
        ring[index] = 0
        for tap in TAPS:
            ring[((i + tap) >> 4) & 63] ^= value
    tail = b"".join(ring[((n + off) >> 4) & 63].to_bytes(16, "little")
                    for off in range(0, RESIDUE, 16))
    transformed_raw = (~zlib.crc32(tail, U32)) & U32
    result = (~reverse_with_matrix(transformed_raw)) & U32
    return zlib.crc32(data[n:], result)


def main():
    zero_poly = sum(1 << exp for exp in EXPONENTS)
    assert remainder(zero_poly, CRC32_REFLECTED_GENERATOR) == 0
    squared = sum(1 << (exp * 128) for exp in EXPONENTS)
    assert remainder(squared, CRC32_REFLECTED_GENERATOR) == 0
    assert TAPS == (16, 48, 112, 144, 192, 208, 448, 592, 624, 704)

    # A single equal bit placed at each tap must have zero *raw* CRC.
    impulses = bytearray(RESIDUE + 1)
    for exp in EXPONENTS:
        impulses[exp * 16] = 1
    assert zlib.crc32(impulses, U32) == U32

    rng = random.Random(0x241216398)
    for _ in range(100):
        value = rng.getrandbits(32)
        # Shift a raw CRC by 704 zero bytes, then invert.
        after = (~zlib.crc32(bytes(RESIDUE), (~value) & U32)) & U32
        assert reverse_with_matrix(after) == value

    lengths = (0, 1, 16, 63, 1023, 1024, 1025, 2048,
               4095, 4096, 4097, 4113, 8191, 8192, 8193,
               16384, 65536, 131072)
    seeds = (0, 1, 0xFFFFFFFF, 0x12345678)
    tested = 0
    for threshold in (1024, 4096, 8192):
        for alignment in (0, 1, 7, 15):
            for length in lengths:
                if threshold != 4096 and length > 16384:
                    continue  # Large-frame default CRC model already covered.
                payload = bytes(rng.getrandbits(8) for _ in range(length))
                for seed in seeds:
                    expected = zlib.crc32(payload, seed)
                    got = chorba_model(seed, payload, alignment, threshold)
                    assert got == expected, (threshold, alignment, length,
                                             hex(seed), hex(got), hex(expected))
                    tested += 1

    print("Reflected GF(2) polynomial: 0x%X" % zero_poly)
    print("MMI XOR taps (bytes):", TAPS)
    print("704-byte inverse CRC32 shift matrix:")
    for i in range(0, 32, 4):
        print("    " + ", ".join("0x%08xu" % x for x in INVERSE[i:i + 4]) + ",")
    print("MMI Chorba polynomial/reference checks: PASS (%d CRC cases)" % tested)


if __name__ == "__main__":
    main()
