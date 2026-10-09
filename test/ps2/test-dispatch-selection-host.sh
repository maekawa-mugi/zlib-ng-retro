#!/bin/sh
set -eu
cc=${CC:-cc}
src=test/ps2/test-dispatch-selection-host.c
common="-std=c11 -O2 -Wall -Wextra -Werror -DMIPS_MMI -DDISABLE_RUNTIME_CPU_DETECTION -DMIPS_MMI_ADLER32 -DMIPS_MMI_CHORBA"
"$cc" $common -DTEST_NO_DISPATCH "$src" -o /tmp/mmi-dispatch-generic
/tmp/mmi-dispatch-generic
"$cc" $common -DTEST_ALL_DISPATCH \
    -DMIPS_MMI_COMPARE256_DISPATCH -DMIPS_MMI_ADLER32_DISPATCH \
    -DMIPS_MMI_ADLER32_COPY_DISPATCH -DMIPS_MMI_CHORBA_DISPATCH \
    -DMIPS_MMI_CHORBA_COPY_DISPATCH "$src" -o /tmp/mmi-dispatch-native
/tmp/mmi-dispatch-native
printf 'MMI production dispatch macro checks: PASS\n'
