#!/usr/bin/env python3
"""Compare two PS2 EE RT_CASE logs without confusing compress and decode.
Input: original-zlib stdout, zlib-ng MMI stdout, in that order.
Ratios >1: zlib-ng MMI has more uncompressed MB/s than original zlib.
Different compressed sizes mean decode figures are end-to-end throughput,
NOT identical compressed-bitstream decoder kernel speed.
"""
import csv
import pathlib
import sys

FIELDS = ("implementation", "pattern", "level", "input_bytes",
          "compressed_bytes", "reps", "c_ticks", "d_ticks", "c_rate", "d_rate")


def parse(path):
    cases = {}
    invalid = []
    version = set()
    for lineno, raw in enumerate(pathlib.Path(path).read_text(
            encoding="utf-8", errors="replace").splitlines(), 1):
        for kind in ("RT_CASE,", "RT_INVALID,", "RT_RESULT,"):
            pos = raw.find(kind)
            if pos >= 0:
                row = next(csv.reader([raw[pos:]]))
                break
        else:
            continue
        if row[0] == "RT_INVALID":
            invalid.append((lineno, row))
            continue
        if row[0] == "RT_RESULT":
            if len(row) < 2 or row[1] != "PASS":
                raise ValueError(f"{path}:{lineno}: incomplete or failed RT_RESULT")
            continue
        if len(row) != 11:
            raise ValueError(f"{path}:{lineno}: expected 11 RT_CASE fields, got {len(row)}")
        label, pattern, level, size, packed, reps, ct, dt, cr, dr = row[1:]
        key = (int(pattern), int(level), int(size))
        if key in cases:
            raise ValueError(f"{path}:{lineno}: duplicate {key}")
        cases[key] = dict(zip(FIELDS, (
            label, int(pattern), int(level), int(size), int(packed),
            int(reps), float(ct), float(dt), float(cr), float(dr))))
        version.add(label)
    if invalid:
        raise ValueError(f"{path}: {len(invalid)} invalid timing cases")
    if len(cases) != 36:
        raise ValueError(f"{path}: only {len(cases)} of expected 36 RT_CASE rows")
    if len(version) != 1:
        raise ValueError(f"{path}: multiple implementations {version}")
    return cases, version.pop()


def compare(stock_path, ng_path):
    original, oa = parse(stock_path)
    optimized, ob = parse(ng_path)
    if oa != "zlib-1.3.2" or ob != "zlib-ng-mmi":
        raise ValueError(f"log order/labels wrong: {oa}, {ob}")
    if original.keys() != optimized.keys():
        raise ValueError("different pattern/level/input-size sets")
    print("PS2 E2E comparison: original zlib 1.3.2 vs zlib-ng MMI")
    print("compress/decode MB/s measured on ORIGINAL source bytes; higher is faster")
    print("Ratios: zlib-ng-MMI / original-zlib (1.00 = same speed)")
    print("P,L,KiB,stock_C_MBps,ng_C_MBps,C_ratio,stock_D_MBps,ng_D_MBps,D_ratio,"
          "stock_packed,ng_packed")
    for key in sorted(original):
        a, b = original[key], optimized[key]
        if a["reps"] != b["reps"]:
            raise ValueError(f"{key}: different timing repetition counts")
        if min(a["c_rate"], b["c_rate"], a["d_rate"], b["d_rate"]) <= 0:
            raise ValueError(f"{key}: invalid MB/s")
        print(f"{key[0]},{key[1]},{key[2]//1024},"
              f"{a['c_rate']:.3f},{b['c_rate']:.3f},"
              f"{b['c_rate']/a['c_rate']:.3f},"
              f"{a['d_rate']:.3f},{b['d_rate']:.3f},"
              f"{b['d_rate']/a['d_rate']:.3f},"
              f"{a['compressed_bytes']},{b['compressed_bytes']}")
    print("CAUTION: each ELF decodes its own compressed output. Packed sizes may "
          "differ; D_ratio is NOT a controlled identical-bitstream decoder comparison.")


def main(argv):
    if len(argv) != 3:
        print("Usage: compare_roundtrip.py stock_zlib.log zlib_ng_mmi.log",file=sys.stderr)
        return 2
    try:
        compare(argv[1], argv[2])
    except (ValueError, OSError) as e:
        print("RT_COMPARE_FAIL:",e,file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
