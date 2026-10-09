#ifndef PS2_BENCH_DISPLAY_H
#define PS2_BENCH_DISPLAY_H
#include <time.h>
#ifdef PS2_BENCH_SCREEN
void ps2_bench_candidates(const char *name, const char *const *names,
                          unsigned count, int competitive);
void ps2_bench_pair(const char *name, const char *a, const char *b);
void ps2_bench_check(unsigned variant, int ok);
void ps2_bench_ticks(unsigned variant, clock_t ticks);
void ps2_bench_screen_init(void);
void ps2_bench_progress(const char *name);
void ps2_bench_finish(int rc);
void ps2_bench_screen_done(unsigned failed);
void ps2_bench_full(int full);
unsigned ps2_bench_iterations(unsigned requested);
void ps2_bench_case(const char *name, unsigned current, unsigned total);
#else
#define ps2_bench_pair(name, a, b) ((void)0)
#define ps2_bench_candidates(name, names, count, competitive) ((void)0)
#define ps2_bench_check(variant, ok) ((void)0)
#define ps2_bench_ticks(variant, ticks) ((void)0)
#define ps2_bench_case(name, current, total) ((void)0)
static inline unsigned ps2_bench_iterations(unsigned requested) { return requested; }
#endif
#endif
