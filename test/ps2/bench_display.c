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

static void draw(void) {
    scr_clear(); scr_setXY(0,0); scr_setfontcolor(WHITE);
    scr_printf("EE MMI auto-compare (%s)\n", full_run ? "full" : "quick");
    scr_printf("Winner is valid only if all candidates pass.\n");
    scr_printf("Same workload, summed clock ticks (lower wins)\n\n");
    for (unsigned i = 0; i < rank_state.count; ++i) {
        const mmi_rank_group *g = &rank_state.groups[i];
        int best = mmi_rank_winner(g);
        scr_setfontcolor(best < 0 ? GRAY : GREEN);
        if (best >= 0) {
            const double ratio = g->v[best].ticks > 0 ?
                g->v[0].ticks / g->v[best].ticks : 0;
            scr_printf("%-15s %-15s x%.2f (%u options)\n",
                       g->name, g->v[best].name, ratio, g->count);
        } else if (!g->competitive) {
            scr_printf("%-15s checks+timing (not A/B)\n", g->name);
        } else {
            scr_printf("%-15s pending / failed / invalid timing\n", g->name);
        }
    }
    scr_setfontcolor(WHITE);
    scr_printf("\n%s\n%s\n", progress, case_progress);
    scr_printf("Independent tests: pass=%u fail=%u\n", tests_ok, tests_failed);
    if (first_failed) { scr_setfontcolor(RED); scr_printf("First fail: %s\n", first_failed); }
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
    progress = failed ? "DONE: FAIL (see log)" : "DONE: tested candidate ranking";
    for (unsigned i = 0; i < rank_state.count; ++i)
        mmi_rank_report(&rank_state.groups[i]);
    draw(); scr_setfontcolor(failed ? RED : GREEN);
    scr_printf("TEST: %s failures=%u\n", failed ? "FAIL!" : "OK", failed);
    ps2_stress_show_error(); fflush(stdout); SleepThread();
}
