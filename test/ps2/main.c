/* Standalone correctness ELF. For integrated checks + speed rankings,
 * build/run mmi_suite.elf (test/mmi_suite.c). */
#include <debug.h>
#include <kernel.h>
#include <stdio.h>
#include "stress_diagnostic.h"

#define UI_WHITE 0x00ffffffu
#define UI_GREEN 0x0000ff00u
#define UI_RED   0x000000ffu
#define UI_YELLOW 0x0000ffffu

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

static unsigned tests_done, tests_failed;
static int run(const char *name, int (*test)(void))
{
    int result;
    int row=5+(int)tests_done;
    scr_setXY(0,row); scr_setfontcolor(UI_YELLOW);
    scr_printf("%-15.15s %-7s",name,"RUN");
    printf("PS2 MMI: running %s\n",name);
    fflush(stdout);
    result=test();
    ++tests_done;
    tests_failed+=result!=0;
    scr_setXY(0,row);
    scr_setfontcolor(result?UI_RED:UI_GREEN);
    scr_printf("%-15.15s %-7s",name,result?"FAIL":"PASS");
    printf("PS2 MMI: %-15s %s (code %d)\n",
           name,result?"FAIL":"PASS",result);
    fflush(stdout);
    return result!=0;
}
int main(void)
{
    int failures=0;
    init_scr();
    scr_setCursor(0);
    scr_setfontcolor(UI_WHITE);
    scr_setXY(0,0);
    scr_printf("ZLIB-NG RETRO | PS2 EE MMI | VALIDATION");
    scr_setXY(0,1);
    scr_printf("Correctness-only ELF | use mmi_suite.elf for speed");
    scr_setXY(0,3);
    scr_printf("%-15s %-7s","FUNCTION","TEST");
    failures+=run("slide_hash",ps2_test_slide_hash);
    failures+=run("compare256",ps2_test_compare256);
    failures+=run("chunkset",ps2_test_chunkset);
    failures+=run("roundtrip",ps2_test_roundtrip);
    failures+=run("stress",ps2_test_stress);
    failures+=run("Adler math",ps2_test_adler32_math);
#ifdef MIPS_MMI_ADLER32
    failures+=run("Adler-32",ps2_test_adler32);
#endif
#ifdef MIPS_MMI_CHORBA
    failures+=run("Chorba CRC",ps2_test_chorba);
#endif
    scr_setXY(0,23);
    scr_setfontcolor(failures?UI_RED:UI_GREEN);
    scr_printf("RESULT: %s | %u/%u TESTS | FAILURES=%u      ",
               failures?"FAIL":"PASS",tests_done-tests_failed,
               tests_done,tests_failed);
    scr_setXY(0,24);
    scr_setfontcolor(UI_WHITE);
    scr_printf("COMPLETE | detailed failures on stdout");
    printf("PS2 MMI RESULT: %s checks=%u failures=%u\n",
           failures?"FAIL":"PASS",tests_done,tests_failed);
    fflush(stdout);
    SleepThread();
    return failures!=0;
}
