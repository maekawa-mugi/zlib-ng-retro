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
static char roundtrip_summary[96] = "ROUNDTRIP MB/s: awaiting verified compress/decode";
static unsigned tests_ok, tests_failed;
static unsigned full_run;
static clock_t last_draw;
static const char *first_failed;
static int initialized;
static int screen_started, finished;
static unsigned final_failures;
static const char *poly_best[3];
static double poly_speed[3];
/* Last measured timings are reported PER condition, not across sizes. */
static clock_t case_ticks[MMI_RANK_VARIANTS];
static unsigned case_seen, case_number;


/* Fixed GS coordinates; no newline/scrolling and no arbitrary diagnostic
 * strings at row 24. A full table is updated while tests and timing run. */
static void draw(void) {
    unsigned i;
    if (!screen_started) {
        scr_clear();
        screen_started = 1;
    }
    scr_setfontcolor(WHITE); scr_setXY(0,0);
#ifdef PS2_STOCK_ZLIB
    scr_printf("ZLIB 1.3.2 ORIGINAL | PS2 EE | ROUNDTRIP BENCHMARK");
#else
    scr_printf("ZLIB-NG RETRO | PS2 EE MMI | VALIDATION + BENCHMARK");
#endif
    scr_setXY(0,1);
#ifdef PS2_STOCK_ZLIB
    scr_printf("Mode: %-5s | 36 identical patterns/levels | RT_CASE CSV",
               full_run ? "FULL" : "QUICK");
#else
    scr_printf("Mode: %-5s | MIXED best is time-weighted; per-case CSV",
               full_run ? "FULL" : "QUICK");
#endif
    scr_setXY(0,2);
    scr_printf("%-15s %-16s %-9s %s", "FUNCTION", "MIXED BEST", "SPEED", "STATE");
    for (i = 0; i < MMI_RANK_GROUPS; ++i) {
        scr_setXY(0,4+(int)i);
        if (i < rank_state.count) {
            const mmi_rank_group *g = &rank_state.groups[i];
            int best = mmi_rank_winner(g);
            if (best >= 0) {
                double ratio = g->v[best].ticks > 0 ?
                    g->v[0].ticks / g->v[best].ticks : 0.0;
                scr_setfontcolor(WHITE);
                scr_printf("%-15.15s %-16.16s %5.2fx    %-8s    ",
                           g->name, g->v[best].name, ratio, "");
                scr_setXY(43,4+(int)i);
                scr_setfontcolor(GREEN);
                scr_printf("PASS");
                scr_setfontcolor(WHITE);
            } else if (!g->competitive) {
                scr_setfontcolor(WHITE);
                scr_printf("%-15.15s %-16s %-9s %-8s     ",
                           g->name, "N/A", "--", "CHECKS");
            } else {
                scr_setfontcolor(WHITE);
                scr_printf("%-15.15s %-16s %-9s %-8s     ",
                           g->name, "PENDING", "--", "CHECKING");
            }
        } else {
            scr_setfontcolor(WHITE);
            scr_printf("%-15s %-16s %-9s %-8s     ",
                       "--", "WAIT", "--", "WAIT");
        }
    }
    scr_setfontcolor(WHITE);
    scr_setXY(0,16);
    scr_printf("%-70.70s",roundtrip_summary);
    scr_setXY(0,17);
    scr_printf("ACTIVE %-55.55s   ",progress);
    scr_setXY(0,18);
    scr_printf("DETAIL %-55.55s   ",case_progress);
    scr_setXY(0,19);
    scr_setfontcolor(WHITE);
    scr_printf("CRC SIZE 4K %-15.15s %5.2fx | 64K %-12.12s %5.2fx      ",
               poly_best[0]?poly_best[0]:"WAIT",poly_speed[0],
               poly_best[1]?poly_best[1]:"WAIT",poly_speed[1]);
    scr_setXY(0,20);
    scr_printf("CORRECTNESS | PASS %3u | FAIL %3u      ",tests_ok,tests_failed);
    scr_setXY(0,21);
    if (first_failed) {
        scr_setfontcolor(WHITE);
        scr_printf("FIRST FAIL: %-48.48s     ",first_failed);
    } else {
        scr_setfontcolor(WHITE);
        scr_printf("First failure: none                                          ");
    }
    scr_setXY(0,22);
    scr_setfontcolor(WHITE);
    scr_printf("CRC SIZE 1M %-15.15s %5.2fx  (all sizes on stdout)     ",
               poly_best[2]?poly_best[2]:"WAIT",poly_speed[2]);
    scr_setXY(0,23);
    scr_setfontcolor(WHITE);
    if (finished) {
        scr_printf("RESULT: %-4s | failed groups %u | %u ranked families     ",
                   "",final_failures,rank_state.count);
        scr_setXY(8,23);
        scr_setfontcolor(final_failures?WHITE:GREEN);
        scr_printf("%-4s",final_failures?"FAIL":"PASS");
    } else
        scr_printf("RESULT: RUNNING | %u benchmark families registered       ",rank_state.count);
    scr_setXY(0,24);
    scr_setfontcolor(WHITE);
    scr_printf("%-54s","Detailed results: MMI_SUITE_* and MMI_* on stdout");
}

void ps2_bench_roundtrip_rate(unsigned pattern, unsigned level, size_t bytes,
                              double compress_mb_s, double decode_mb_s)
{
    /* Never present these independent phases as competing kernels. */
    snprintf(roundtrip_summary,sizeof(roundtrip_summary),
             "RT P%u L%u %luKiB | C %.2f MB/s | D %.2f MB/s",
             pattern,level,(unsigned long)(bytes/1024U),
             compress_mb_s,decode_mb_s);
    draw();
}
void ps2_bench_poly_highlight(unsigned size_slot, const char *winner,
                              double braid_over_best) {
    if(size_slot>=3 || braid_over_best<=0.0) return;
    poly_best[size_slot]=winner;
    poly_speed[size_slot]=braid_over_best;
    draw();
}
void ps2_bench_screen_init(void) {
    if (!initialized) { mmi_rank_reset(&rank_state); initialized = 1; }
    init_scr(); scr_setCursor(0); draw();
}
void ps2_bench_progress(const char *name) {
    rank_state.active = -1;
    case_number=case_seen=0;
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
    /* One exact condition has the same iteration count and input for all
     * candidates. Emit its winner separately from mixed-size aggregates. */
    if (case_number != 0 && rank_state.active >= 0) {
        const mmi_rank_group *g=&rank_state.groups[rank_state.active];
        if (i < g->count && i < MMI_RANK_VARIANTS) {
            case_ticks[i]=ticks;
            case_seen|=(1U<<i);
            if (case_seen == (1U<<g->count)-1U) {
                int best=mmi_rank_case_winner(g,case_ticks);
                if (g->competitive && best>=0) {
                    double ratio=(double)case_ticks[0]/(double)case_ticks[best];
                    printf("MMI_CASE_WINNER,%s,%u,%s,%.4f,%ld,%ld\n",
                           g->name,case_number,g->v[best].name,ratio,
                           (long)case_ticks[0],(long)case_ticks[best]);
                } else if (g->competitive) {
                    printf("MMI_CASE_WINNER,%s,%u,UNDETERMINED\n",
                           g->name,case_number);
                }
                case_seen=0;
                case_number=0;
            }
        }
    }
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
    case_number=case_seen=0;
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
    case_number=current;
    case_seen=0;
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
