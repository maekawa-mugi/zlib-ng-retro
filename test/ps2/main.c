#include <debug.h>
#include <kernel.h>
#include <stdio.h>
#include "stress_diagnostic.h"

int ps2_test_slide_hash(void);
int ps2_test_compare256(void);
int ps2_test_chunkset(void);
int ps2_test_roundtrip(void);
int ps2_test_stress(void);
int ps2_test_adler32_math(void);
#ifdef MIPS_MMI_ADLER32
int ps2_test_adler32(void);
#endif
#ifdef MIPS_MMI_CHORBA
int ps2_test_chorba(void);
#endif

static int run(const char *name, int (*test)(void)) {
    scr_printf("%-12s RUNNING\n", name);
    printf("PS2 MMI: running %s\n", name);
    int result = test();
    scr_printf("             %s (code %d)\n", result ? "FAIL" : "PASS", result);
    printf("PS2 MMI: %s %s (code %d)\n", name, result ? "FAIL" : "PASS", result);
    fflush(stdout);
    return result != 0;
}

int main(void) {
    init_scr();
    scr_setCursor(0);
    scr_printf("zlib-ng PS2 EE MMI regression tests\n\n");
#ifdef MIPS_MMI_COMPARE64
    scr_printf("64-byte compare: enabled\n");
#endif
    int failures = 0;
    failures += run("slide_hash", ps2_test_slide_hash);
    failures += run("compare256", ps2_test_compare256);
    failures += run("chunkset", ps2_test_chunkset);
    failures += run("roundtrip", ps2_test_roundtrip);
    failures += run("stress", ps2_test_stress);
    failures += run("Adler math", ps2_test_adler32_math);
#ifdef MIPS_MMI_ADLER32
    failures += run("Adler-32", ps2_test_adler32);
#endif
#ifdef MIPS_MMI_CHORBA
    failures += run("Chorba CRC", ps2_test_chorba);
#endif
    const char *result = failures ? "TEST: FAIL!" : "TEST: OK!";
    scr_printf("\n%s  failures=%d\n", result, failures);
    printf("%s failures=%d\n", result, failures);
    fflush(stdout);
    ps2_stress_show_error();
    // Keep the result visible until the emulator is stopped.
    SleepThread();
    return failures != 0;
}

