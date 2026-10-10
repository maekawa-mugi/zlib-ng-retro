#!/bin/sh
# Build the ORIGINAL vendored zlib 1.3.2 source and the EXACT PS2
# roundtrip benchmark body with a lightweight portable GS adapter.
set -eu
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
src="third_party/zlib-1.3.2"
for base in adler32 crc32 compress uncompr deflate inflate inftrees inffast trees zutil; do
    "$CC" -std=c11 -O2 -Wall -Wextra -Werror -I"$src" \
        -c "$src/$base.c" -o "$tmp/$base.o"
done
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I. -I"$src" \
    -DPS2_STOCK_ZLIB -DPS2_BENCH_SCREEN \
    -Dmain=stock_roundtrip_host_main \
    -c test/bench_mmi_roundtrip.c -o "$tmp/roundtrip.o"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Itest/ps2 -I. \
    -c test/ps2/test_stock_roundtrip_host.c -o "$tmp/host.o"
"$CC" "$tmp"/*.o -o "$tmp/stock_test"
"$tmp/stock_test" > "$tmp/roundtrip.log"
grep -q '^STOCK_HOST_PASS,36/36,original-zlib-1.3.2$' "$tmp/roundtrip.log"
grep -q '^RT_RESULT,PASS,valid=36,invalid=0,expected=36,' "$tmp/roundtrip.log"
test "$(grep -c '^RT_CASE,zlib-1.3.2,' "$tmp/roundtrip.log")" -eq 36
test "$(grep -c '^RT_META,zlib-1.3.2,' "$tmp/roundtrip.log")" -eq 1
echo "PASS: vendored stock zlib 1.3.2, 36/36 cross-platform RT_CASE with identical benchmark core"
