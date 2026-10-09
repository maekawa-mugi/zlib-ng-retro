#!/usr/bin/env python3
"""Independent GF(2) identities and reflected CRC32 inverse transforms.

Uses only Python stdlib; validates the published zero-polynomial tap sets
AND the 32-column inverse-shift matrices encoded in crc_poly_mmi.c.
R5900 16-byte LQ/PXOR/SQ groups correspond to raising Z(x) to 128.
"""
from __future__ import annotations
import pathlib
import re

REFLECTED_GENERATOR = 0x1DB710641
REFLECTED_STEP = 0xEDB88320
SPECS = [
    ("gen32", [i for i in range(33) if REFLECTED_GENERATOR & (1 << i)]),
    ("chorba352", [44, 39, 37, 28, 13, 12, 9, 7, 3, 1, 0]),
    ("small300", [300, 211, 183, 145, 0]),
    ("small600", [600, 422, 366, 290, 0]),
    ("sparse4_3006", [3006, 791, 140, 0]),
    # Paper prints forward CRC representation; reverse exponents.
    ("dense4_5869", [5869, 48, 34, 0]),
    ("dense5_14870", [14870, 22, 11, 7, 0]),
    ("sparse3_91639", [91639, 49961, 0]),
]

def polynomial_mod(terms: list[int]) -> int:
    poly = 0
    for term in terms:
        poly ^= 1 << term
    for bit in range(poly.bit_length() - 1, 31, -1):
        if (poly >> bit) & 1:
            poly ^= REFLECTED_GENERATOR << (bit - 32)
    return poly

def inv_byte(crc: int) -> int:
    for _ in range(8):
        high = (crc >> 31) & 1
        crc = (((crc ^ (REFLECTED_STEP if high else 0)) << 1) | high) & 0xffffffff
    return crc

def apply(cols: list[int], value: int) -> int:
    result = 0
    for bit in range(32):
        if (value >> bit) & 1:
            result ^= cols[bit]
    return result

def compose(a: list[int], b: list[int]) -> list[int]:
    return [apply(a, x) for x in b]

def unshift_columns(nbytes: int) -> list[int]:
    cols = [inv_byte(1 << bit) for bit in range(32)]
    acc = [1 << bit for bit in range(32)]
    while nbytes:
        if nbytes & 1:
            acc = compose(cols, acc)
        cols = compose(cols, cols)
        nbytes >>= 1
    return acc

def run() -> None:
    path = pathlib.Path(__file__).with_name("crc_poly_mmi.c")
    source = path.read_text(encoding="utf-8")
    tested = 0
    for name, terms in SPECS:
        assert polynomial_mod(terms) == 0, "invalid GF(2) identity: " + name
        degree = max(terms)
        horizon = degree * 16
        # Extract one initializer between adjacent names; no additional
        # dependencies on runtime C layout beyond the fixed descriptor.
        m = re.search(r'\{ "' + re.escape(name) +
                      r'", (\d+)u, (\d+)u, (\d+)u, (\d+)u,'
                      r'\s*\{([^}]+)\}\s*,\s*\{([^}]+)\}\s*\}',
                      source, flags=re.S)
        assert m, "missing candidate descriptor: " + name
        got_degree, ring, minimum, tapcount = map(int, m.group(1, 2, 3, 4))
        assert got_degree == degree
        assert tapcount == len(terms) - 1
        assert ring >= horizon + 16 and ring & (ring - 1) == 0
        assert minimum >= horizon * 2
        taps = [int(x) for x in re.findall(r'(\d+)u', m.group(5))]
        assert taps[:tapcount] == [i * 16 for i in sorted(terms) if i != 0]
        encoded = [int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]+)u',
                                                   m.group(6))]
        assert len(encoded) == 32, name
        assert encoded == unshift_columns(horizon), name
        print(f"CRC_POLY_GF2,PASS,{name},degree={degree},terms={len(terms)}")
        tested += 1
    assert tested == 8
    print(f"CRC_POLY_GF2_RESULT,PASS,{tested}")

if __name__ == "__main__":
    run()
