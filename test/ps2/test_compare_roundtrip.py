#!/usr/bin/env python3
"""Host-only checks for two-ELF 36-case CSV comparison."""
import contextlib
import io
import pathlib
import tempfile
from compare_roundtrip import compare, parse


def make(path, label, scale):
    with path.open("w", encoding="utf-8") as out:
        out.write(f"RT_META,{label},1000000,6,uncompressed_MB/s,clock()\n")
        for p in range(4):
            for size in (4096, 65536, 262144):
                for level in (1, 6, 9):
                    packed = size // 2 if p else size + 32
                    out.write(
                        f"RT_CASE,{label},{p},{level},{size},{packed},12,"
                        f"600.0,250.0,{4*scale:.6f},{10*scale:.6f}\n"
                    )
        out.write("RT_RESULT,PASS,valid=36,invalid=0,expected=36,checksum=1\n")


def main():
    with tempfile.TemporaryDirectory() as directory:
        left = pathlib.Path(directory) / "stock.log"
        right = pathlib.Path(directory) / "ng.log"
        make(left, "zlib-1.3.2", 1)
        make(right, "zlib-ng-mmi", 2)
        cases, label = parse(left)
        assert len(cases) == 36 and label == "zlib-1.3.2"
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            compare(left, right)
        assert out.getvalue().count("\n") == 40
        assert ",2.000," in out.getvalue()
        original = right.read_text()
        right.write_text(original.replace("RT_RESULT,PASS", "RT_RESULT,PARTIAL"))
        try:
            compare(left, right)
        except ValueError:
            pass
        else:
            raise AssertionError("incomplete benchmark accepted")
        right.write_text(original.replace("RT_CASE,zlib-ng-mmi,0,1,4096,",
                                          "RT_CASE,zlib-ng-mmi,0,1,8192,"))
        try:
            compare(left, right)
        except ValueError:
            pass
        else:
            raise AssertionError("mismatched input size accepted")
    print("PASS: 36-case stock/NG log comparison and strict mismatch/partial rejection")


if __name__ == "__main__":
    main()
