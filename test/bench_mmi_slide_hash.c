/* Compare serial and interleaved EE MMI slide_hash on the same hardware.
 * Correctness is checked against a scalar reference before timing.
 * clock() can be too coarse on PS2; use an EE cycle counter for final data.
 */
#include "zbuild.h"
#include "deflate.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ps2/bench_display.h"

#ifndef MIPS_MMI
#  error "This benchmark requires WITH_MMI=ON"
#endif

void Z_INTERNAL slide_hash_mmi_serial(deflate_state *s);
void Z_INTERNAL slide_hash_mmi_interleaved(deflate_state *s);
void Z_INTERNAL slide_hash_mmi_interleaved2(deflate_state *s);

static Pos head[HASH_SIZE + 16] ALIGNED_(16);
static Pos prev[32768 + 16] ALIGNED_(16);
static Pos expected_head[HASH_SIZE];
static Pos expected_prev[32768];
static deflate_state state;
static volatile uint32_t sink;

typedef void (*slide_fn)(deflate_state *);

static uint32_t mix(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    return x ^ (x << 5);
}

static void fill_tables(deflate_state *s) {
    uint32_t seed = 0x9e3779b9u;
    for (uint32_t i = 0; i < HASH_SIZE; ++i) {
        seed = mix(seed);
        s->head[i] = (Pos)seed;
    }
    for (uint32_t i = 0; i < s->w_size; ++i) {
        seed = mix(seed);
        s->prev[i] = (Pos)seed;
    }
}

static void scalar_slide(Pos *array, uint32_t count, Pos wsize) {
    for (uint32_t i = 0; i < count; ++i) {
        Pos value = array[i];
        array[i] = value >= wsize ? value - wsize : 0;
    }
}

/* Explicit scalar C candidate. It is an independent oracle and is
 * deliberately part of the timed competition, not just validation. */
static void slide_hash_bench_c(deflate_state *s) {
    Pos wsize = (Pos)s->w_size;
    scalar_slide(s->head, HASH_SIZE, wsize);
    scalar_slide(s->prev, wsize, wsize);
}
static int validate(deflate_state *s, slide_fn fn) {
    Pos wsize = (Pos)s->w_size;
    fill_tables(s);
    memcpy(expected_head, s->head, sizeof(expected_head));
    memcpy(expected_prev, s->prev, (size_t)wsize * sizeof(Pos));
    scalar_slide(expected_head, HASH_SIZE, wsize);
    scalar_slide(expected_prev, wsize, wsize);
    fn(s);
    return memcmp(expected_head, s->head, sizeof(expected_head)) == 0 &&
           memcmp(expected_prev, s->prev, (size_t)wsize * sizeof(Pos)) == 0;
}

static clock_t run(deflate_state *s, slide_fn fn, unsigned iterations) {
    clock_t ticks = 0;
    /* Slide is destructive. Restore the same nontrivial hash data OUTSIDE
     * each measured call, otherwise nearly every timed call sees zeros. */
    for (unsigned i = 0; i < iterations; ++i) {
        fill_tables(s);
        clock_t before = clock();
        fn(s);
        clock_t after = clock();
        if (before == (clock_t)-1 || after == (clock_t)-1 || after < before)
            return (clock_t)-1;
        ticks += after - before;
        sink ^= s->head[i % HASH_SIZE] ^ s->prev[i % s->w_size];
    }
    return ticks;
}

int main(void) {
    static const char *const names[] = {"scalar", "serial", "load2", "load4"};
    static const slide_fn variants[] = {
        slide_hash_bench_c, slide_hash_mmi_serial,
        slide_hash_mmi_interleaved2, slide_hash_mmi_interleaved
    };
    ps2_bench_candidates("slide_hash", names, 4, 1);
    static const unsigned offsets[] = {0, 1, 7};
    static const unsigned sizes[] = {1024, 32768};
    const unsigned iterations = ps2_bench_iterations(500);

    printf("MMI slide_hash A/B CLOCKS_PER_SEC=%lu\n",
           (unsigned long)CLOCKS_PER_SEC);
    puts("wsize offset iterations scalar_ticks serial_ticks load2_ticks load4_ticks");
    for (unsigned si = 0; si < sizeof(sizes)/sizeof(sizes[0]); ++si) {
        for (unsigned oi = 0; oi < sizeof(offsets)/sizeof(offsets[0]); ++oi) {
            ps2_bench_case("slide_hash cases", si * 3 + oi + 1, 6);
                unsigned offset = offsets[oi];
            state.w_size = sizes[si];
            state.head = head + offset;
            state.prev = prev + offset;
            for (unsigned v = 0; v < 4; ++v) {
                int ok = validate(&state, variants[v]);
                ps2_bench_check(v, ok);
                if (!ok) {
                    printf("slide_hash FAIL wsize=%u offset=%u variant=%u\n",
                           sizes[si], offset, v);
                    return 1;
                }
            }
            clock_t timings[4];
            if ((si + oi) & 1u) {
                for (int v = 3; v >= 0; --v)
                    timings[v] = run(&state, variants[v], iterations);
            } else {
                for (unsigned v = 0; v < 4; ++v)
                    timings[v] = run(&state, variants[v], iterations);
            }
            for (unsigned v = 0; v < 4; ++v) ps2_bench_ticks(v, timings[v]);
            printf("%u %u %u %ld %ld %ld %ld\n", sizes[si], offset,
                   iterations, (long)timings[0], (long)timings[1],
                   (long)timings[2], (long)timings[3]);
        }
    }
    printf("benchmark sink=%lu\n", (unsigned long)sink);
    return 0;
}
