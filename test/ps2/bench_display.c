/* Real EE screen. The portable rank core is separately host-tested. */
#include "bench_display.h"
#include "bench_rank.h"
#include <debug.h>
#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include "stress_diagnostic.h"

#define WHITE 0x00ffffffu
#define GREEN 0x0000ff00u
#define RED   0x000000ffu
#define GRAY  0x00808080u
static mmi_rank_state rank_state;
static const char *progress = "Starting";
static char case_progress[96];
static unsigned tests_ok, tests_failed;
static unsigned full_run;
static clock_t last_draw;
static const char *first_failed;
static int initialized;
static int screen_started, finished;
static unsigned final_failures;

/* Fixed GS coordinates; no newline/scrolling and no arbitrary diagnostic
 * strings at row 24. A full table is updated while tests and timing run. */
static void draw(void) {
    unsigned i;
    if (!screen_started) {
        scr_clear();
        screen_started = 1;
    }
    scr_setfontcolor(WHITE); scr_setXY(0,0);
    scr_printf("ZLIB-NG RETRO | PS2 EE MMI | VALIDATION + BENCHMARK");
    scr_setXY(0,1);
    scr_printf("Mode: %-5s | every winner requires passed checks + timing",
               full_run ? "FULL" : "QUICK");
    scr_setXY(0,2);
    scr_printf("%-15s %-16s %-9s %s", "FUNCTION", "PROVISIONAL BEST", "SPEED", "STATE");
    for (i = 0; i < MMI_RANK_GROUPS; ++i) {
        scr_setXY(0,4+(int)i);
        if (i < rank_state.count) {
            const mmi_rank_group *g = &rank_state.groups[i];
            int best = mmi_rank_winner(g);
            if (best >= 0) {
                double ratio = g->v[best].ticks > 0 ?
                    g->v[0].ticks / g->v[best].ticks : 0.0;
                scr_setfontcolor(GREEN);
                scr_printf("%-15.15s %-16.16s %5.2fx    %-8s    ",
                           g->name, g->v[best].name, ratio, "PASS");
            } else if (!g->competitive) {
                scr_setfontcolor(WHITE);
                scr_printf("%-15.15s %-16s %-9s %-8s     ",
                           g->name, "N/A", "--", "CHECKS");
            } else {
                scr_setfontcolor(GRAY);
                scr_printf("%-15.15s %-16s %-9s %-8s     ",
                           g->name, "PENDING", "--", "CHECKING");
            }
        } else {
            scr_setfontcolor(GRAY);
            scr_printf("%-15s %-16s %-9s %-8s     ",
                       "--", "WAIT", "--", "WAIT");
        }
    }
    scr_setfontcolor(WHITE);
    scr_setXY(0,17);
    scr_printf("ACTIVE %-55.55s   ",progress);
    scr_setXY(0,18);
    scr_printf("DETAIL %-55.55s   ",case_progress);
    scr_setXY(0,20);
    scr_printf("CORRECTNESS | PASS %3u | FAIL %3u      ",tests_ok,tests_failed);
    scr_setXY(0,21);
    if (first_failed) {
        scr_setfontcolor(RED);
        scr_printf("FIRST FAIL: %-48.48s     ",first_failed);
    } else {
        scr_setfontcolor(WHITE);
        scr_printf("First failure: none                                          ");
    }
    scr_setXY(0,23);
    scr_setfontcolor(finished ? (final_failures ? RED : GREEN) : WHITE);
    if (finished)
        scr_printf("RESULT: %s | failed groups %u | %u ranked families     ",
                   final_failures ? "FAIL" : "PASS",final_failures,rank_state.count);
    else
        scr_printf("RESULT: RUNNING | %u benchmark families registered       ",rank_state.count);
    scr_setXY(0,24);
    scr_setfontcolor(WHITE);
    scr_printf("%-54s","Detailed results: MMI_SUITE_* and MMI_* on stdout");
}

void ps2_bench_screen_init(void) {
    if (!initialized) { mmi_rank_reset(&rank_state); initialized = 1; }
    init_scr(); scr_setCursor(0); draw();
}
void ps2_bench_progress(const char *name) {
    rank_state.active = -1;
    progress = name; case_progress[0] = 0; draw();
}
void ps2_bench_candidates(const char *name, const char *const *names,
                          unsigned n, int competitive) {
    if (!initialized) { mmi_rank_reset(&rank_state); initialized = 1; }
    if (mmi_rank_start(&rank_state, name, names, n, competitive) < 0) {
        if (!first_failed) first_failed = "candidate registration capacity";
    }
    draw();
}
void ps2_bench_pair(const char *name, const char *a, const char *b) {
    const char *names[2] = {a,b};
    ps2_bench_candidates(name, names, 2, strcmp(name,"roundtrip phases") != 0);
}
void ps2_bench_check(unsigned i, int ok) {
    mmi_rank_check(&rank_state, i, ok);
    if (!ok) draw();
}
void ps2_bench_ticks(unsigned i, clock_t ticks) {
    mmi_rank_ticks(&rank_state, i, ticks);
    clock_t now = clock();
    if (now == (clock_t)-1 || last_draw == (clock_t)-1 ||
        now - last_draw >= CLOCKS_PER_SEC / 4) { draw(); last_draw = now; }
}
void ps2_bench_finish(int rc) {
    if (rc && !first_failed) first_failed = progress;
    if (strncmp(progress,"test_",5) == 0) {
        if (rc) ++tests_failed; else ++tests_ok;
    }
    rank_state.active = -1;
    draw();
}
void ps2_bench_full(int full) { full_run = full != 0; }
unsigned ps2_bench_iterations(unsigned requested) {
    if (full_run) return requested;
    unsigned n = requested / 20;
    return n ? n : 1;
}
void ps2_bench_case(const char *name, unsigned current, unsigned total) {
    snprintf(case_progress, sizeof(case_progress), "%s %u/%u", name, current, total);
    clock_t now = clock();
    if (now == (clock_t)-1 || last_draw == (clock_t)-1 ||
        now - last_draw >= CLOCKS_PER_SEC / 4) {draw(); last_draw = now;}
}
void ps2_bench_screen_done(unsigned failed) {
    progress = failed ? "COMPLETE - FAIL" : "COMPLETE - PASS";
    finished = 1;
    final_failures = failed;
    for (unsigned i = 0; i < rank_state.count; ++i)
        mmi_rank_report(&rank_state.groups[i]);
    draw();
    printf("PS2 EE MMI RESULT: %s failures=%u\n",
           failed ? "FAIL" : "PASS",failed);
    fflush(stdout);
    SleepThread();
}
