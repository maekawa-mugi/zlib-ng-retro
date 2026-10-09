#!/usr/bin/env python3
"""Compare PS2 EE unified-suite logs across compile-time kernel variants.

Run on the development PC AFTER retrieving the entire results/ folder from
PS2 Linux. Only Python 3's standard library is required. Never run on PS2.
"""
import csv
import sys
from pathlib import Path


def read_run(path):
    """Return end-to-end (pattern,level,input) timings and failure records."""
    values = {}
    failures = []
    summary = None
    in_roundtrip = False
    for raw in path.read_text(errors="replace").splitlines():
        line = raw.strip()
        if line.startswith("MMI_SUITE_BEGIN,"):
            parts = line.split(",")
            in_roundtrip = (len(parts) >= 3 and parts[1:] == ["bench", "bench_roundtrip"])
            continue
        if line.startswith("MMI_SUITE_RESULT,"):
            parts = line.split(",")
            if len(parts) >= 5 and parts[3] != "PASS":
                failures.append(parts[1] + "/" + parts[2])
            in_roundtrip = False
            continue
        if line.startswith("MMI_SUITE_SUMMARY,"):
            summary = line
            continue
        if not in_roundtrip:
            continue
        parts = line.split()
        # pattern level input_size compressed_size iterations
        # compress_ticks decompress_ticks
        if len(parts) != 7 or not all(part.isdigit() for part in parts):
            continue
        try:
            pattern, level, length, packed, iters, c, u = map(int, parts)
        except ValueError:
            continue
        if c <= 0 or u <= 0:
            continue
        values[(pattern, level, length)] = (packed, iters, c, u)
    return values, failures, summary


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 test/ee-summarize-results.py /path/to/ee-mmi-bundle",
              file=sys.stderr)
        return 2
    bundle = Path(sys.argv[1])
    manifest = bundle / "manifest.csv"
    if not manifest.is_file():
        print("Cannot read " + str(manifest), file=sys.stderr)
        return 2
    variants = []
    with manifest.open(newline="") as f:
        for row in csv.DictReader(f):
            variant, feature, elf = row["variant"], row["feature"], row["elf"]
            log = bundle / "results" / (elf + "_benches.log")
            if not log.is_file():
                log = bundle / "results" / (elf + "_all.log")
            if not log.is_file():
                print("Missing benchmark log for " + variant, file=sys.stderr)
                continue
            measurements, failures, summary = read_run(log)
            # When tests and benches were run separately, always inspect
            # correctness failures from the test log as well.
            test_log = bundle / "results" / (elf + "_tests.log")
            if test_log.is_file() and test_log != log:
                _, test_failures, test_summary = read_run(test_log)
                failures.extend(test_failures)
                if test_summary is None:
                    failures.append("test/incomplete_log")
            variants.append((variant, feature, measurements, failures, summary, log))
    control = next((v[2] for v in variants if v[0] == "baseline"), None)
    if not control:
        print("No usable baseline timing rows. Check clock() support on EE.",
              file=sys.stderr)
        return 1
    out = bundle / "comparison.csv"
    with out.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["variant", "feature", "pattern", "level", "input_bytes",
                    "compressed_bytes", "iterations", "compress_ticks",
                    "decompress_ticks", "compress_speedup",
                    "decompress_speedup"])
        for variant, feature, measurements, failures, summary, log in variants:
            if failures:
                print("FAIL " + variant + ": " + ", ".join(failures),
                      file=sys.stderr)
            if summary is None:
                print("Warning: incomplete log for " + variant, file=sys.stderr)
            for key, val in sorted(measurements.items()):
                if key not in control:
                    continue
                packed, iters, c, u = val
                _, _, base_c, base_u = control[key]
                w.writerow([variant, feature, *key, packed, iters, c, u,
                            "%.4f" % (base_c / c),
                            "%.4f" % (base_u / u)])
    print("Wrote " + str(out))
    print("A speedup greater than 1.0 indicates a faster variant.")
    print("Always inspect MMI_SUITE_RESULT lines before adopting any candidate.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
