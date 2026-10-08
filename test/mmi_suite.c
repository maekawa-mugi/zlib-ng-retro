/* Unified R5900 PS2 Linux validation and benchmark runner.
 * One executable runs all linked native MMI tests and A/B benchmarks.
 * Emit parseable MMI_SUITE_* records on stdout, suitable for serial/TCP logs.
 */
#include "zbuild.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef MIPS_MMI
#  error "mmi_suite requires WITH_MMI=ON"
#endif

typedef int (*suite_func)(void);
typedef struct {
    const char *name;
    const char *category;
    suite_func run;
    int smoke;
} suite_entry;

#define DECL(kind, id) extern int mmi_suite_##kind##_##id##_main(void)
#define ENTRY(kind, id, is_smoke) { #kind "_" #id, #kind, mmi_suite_##kind##_##id##_main, is_smoke }

DECL(test, slide_hash);
DECL(test, compare256);
DECL(test, chunkset);
DECL(test, roundtrip);
DECL(test, adler32_math);
DECL(bench, slide_hash);
DECL(bench, chunkset);
DECL(bench, roundtrip);
#ifdef MIPS_MMI_COMPARE64
DECL(bench, compare256);
#endif
#ifdef MIPS_MMI_ADLER32
DECL(test, adler32);
DECL(bench, adler32);
DECL(bench, adler32_copy);
#endif
#ifdef MIPS_MMI_CHORBA
DECL(test, chorba);
DECL(bench, chorba);
DECL(bench, chorba_copy);
#endif

static const suite_entry entries[] = {
    ENTRY(test, slide_hash, 1),
    ENTRY(test, compare256, 1),
    ENTRY(test, chunkset, 1),
    ENTRY(test, roundtrip, 1),
    ENTRY(test, adler32_math, 1),
#ifdef MIPS_MMI_ADLER32
    ENTRY(test, adler32, 0),
#endif
#ifdef MIPS_MMI_CHORBA
    ENTRY(test, chorba, 0),
#endif
    ENTRY(bench, slide_hash, 0),
    ENTRY(bench, chunkset, 0),
    ENTRY(bench, roundtrip, 0),
#ifdef MIPS_MMI_COMPARE64
    ENTRY(bench, compare256, 0),
#endif
#ifdef MIPS_MMI_ADLER32
    ENTRY(bench, adler32, 0),
    ENTRY(bench, adler32_copy, 0),
#endif
#ifdef MIPS_MMI_CHORBA
    ENTRY(bench, chorba, 0),
    ENTRY(bench, chorba_copy, 0),
#endif
};

#define ISSET(name) ((name) ? "ON" : "OFF")

static void usage(const char *name) {
    printf("Usage: %s [--all|--tests|--benches|--smoke|--list|--only NAME] [--fail-fast]\n", name);
    puts("  --all       Run all correctness tests, then all benchmarks (default)");
    puts("  --tests     Run every correctness test");
    puts("  --benches   Run every A/B benchmark");
    puts("  --smoke     Run essential correctness tests without benchmarks");
    puts("  --list      Print linked tests and benchmarks");
    puts("  --only NAME Run one entry from --list, e.g. test_compare256");
    puts("  --fail-fast Stop after the first failing entry (default continues)");
}

static void features(void) {
    puts("MMI_SUITE_VERSION,1");
#ifdef MIPS_MMI_COMPARE64
    puts("MMI_SUITE_FEATURE,compare64,ON");
#else
    puts("MMI_SUITE_FEATURE,compare64,OFF");
#endif
#ifdef MIPS_MMI_COMPARE_SWAR
    puts("MMI_SUITE_FEATURE,compare_swar,ON");
#else
    puts("MMI_SUITE_FEATURE,compare_swar,OFF");
#endif
#ifdef MIPS_MMI_SLIDE_HASH_INTERLEAVED
    puts("MMI_SUITE_FEATURE,slide_interleaved,ON");
#else
    puts("MMI_SUITE_FEATURE,slide_interleaved,OFF");
#endif
#ifdef MIPS_MMI_CHUNKSET_BURST
    puts("MMI_SUITE_FEATURE,chunkset_burst,ON");
#else
    puts("MMI_SUITE_FEATURE,chunkset_burst,OFF");
#endif
#ifdef MIPS_MMI_ADLER32
    puts("MMI_SUITE_FEATURE,adler32,ON");
#else
    puts("MMI_SUITE_FEATURE,adler32,OFF");
#endif
#ifdef MIPS_MMI_ADLER32_FORMULA
    puts("MMI_SUITE_FEATURE,adler_formula,ON");
#else
    puts("MMI_SUITE_FEATURE,adler_formula,OFF");
#endif
#ifdef MIPS_MMI_ADLER32_FUSED_COPY
    puts("MMI_SUITE_FEATURE,adler_fused_copy,ON");
#else
    puts("MMI_SUITE_FEATURE,adler_fused_copy,OFF");
#endif
#ifdef MIPS_MMI_CHORBA
    puts("MMI_SUITE_FEATURE,chorba,ON");
#else
    puts("MMI_SUITE_FEATURE,chorba,OFF");
#endif
#ifdef MIPS_MMI_CHORBA_PAIRED_TAPS
    puts("MMI_SUITE_FEATURE,chorba_paired_taps,ON");
#else
    puts("MMI_SUITE_FEATURE,chorba_paired_taps,OFF");
#endif
#ifdef MIPS_MMI_CHORBA_FUSED_COPY
    puts("MMI_SUITE_FEATURE,chorba_fused_copy,ON");
#else
    puts("MMI_SUITE_FEATURE,chorba_fused_copy,OFF");
#endif
    printf("MMI_SUITE_CLOCKS_PER_SEC,%lu\n", (unsigned long)CLOCKS_PER_SEC);
}

int main(int argc, char **argv) {
    enum { ALL, TESTS, BENCHES, SMOKE, LIST, ONLY } mode = ALL;
    const char *only_name = NULL;
    int fail_fast = 0;
    unsigned passed = 0, failed = 0, skipped = 0, total = 0;
    const size_t count = sizeof(entries) / sizeof(entries[0]);

    for (int arg = 1; arg < argc; ++arg) {
        const char *v = argv[arg];
        if (strcmp(v, "--all") == 0) mode = ALL;
        else if (strcmp(v, "--tests") == 0) mode = TESTS;
        else if (strcmp(v, "--benches") == 0) mode = BENCHES;
        else if (strcmp(v, "--smoke") == 0) mode = SMOKE;
        else if (strcmp(v, "--list") == 0) mode = LIST;
        else if (strcmp(v, "--fail-fast") == 0) fail_fast = 1;
        else if (strcmp(v, "--only") == 0 && arg + 1 < argc) {
            mode = ONLY;
            only_name = argv[++arg];
        } else if (strcmp(v, "--help") == 0 || strcmp(v, "-h") == 0) {
            usage(argv[0]); return 0;
        } else {
            fprintf(stderr, "Invalid argument: %s\n", v);
            usage(argv[0]); return 2;
        }
    }

    features();
    if (mode == LIST) {
        for (size_t i = 0; i < count; ++i)
            printf("MMI_SUITE_ENTRY,%s,%s\n", entries[i].category, entries[i].name);
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        const suite_entry *item = &entries[i];
        int selected = mode == ALL ||
            (mode == TESTS && strcmp(item->category, "test") == 0) ||
            (mode == BENCHES && strcmp(item->category, "bench") == 0) ||
            (mode == SMOKE && item->smoke) ||
            (mode == ONLY && strcmp(item->name, only_name) == 0);
        if (!selected) { ++skipped; continue; }
        ++total;
        printf("MMI_SUITE_BEGIN,%s,%s\n", item->category, item->name);
        fflush(stdout);
        clock_t start = clock();
        int rc = item->run();
        clock_t end = clock();
        const long ticks = (start == (clock_t)-1 || end == (clock_t)-1) ?
                           -1L : (long)(end - start);
        if (rc == 0) ++passed; else ++failed;
        printf("MMI_SUITE_RESULT,%s,%s,%s,%d,%ld\n",
               item->category, item->name, rc == 0 ? "PASS" : "FAIL", rc, ticks);
        fflush(stdout);
        if (fail_fast && rc != 0) break;
    }

    printf("MMI_SUITE_SUMMARY,%u,%u,%u,%u\n", total, passed, failed, skipped);
    fflush(stdout);
    return failed ? 1 : total == 0 ? 2 : 0;
}
