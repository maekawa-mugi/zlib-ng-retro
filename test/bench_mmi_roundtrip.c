/* End-to-end PS2 EE deflate/inflate timings and full data verification.
 * Cross-build the same source and flags with individual WITH_MMI_* A/B
 * switches. Timing uses clock(); EE performance counters are preferable.
 */
/* Both PS2 ELFs compile THIS exact benchmark source. The stock build
 * links the unmodified vendored zlib 1.3.2; the other links zlib-ng MMI.
 * Keeping input generation/timing here prevents benchmark drift. */
#ifdef PS2_STOCK_ZLIB
#  include "third_party/zlib-1.3.2/zlib.h"
   typedef uLongf z_uintmax_t;
#  define PREFIX(name) name
#  define RT_IMPL_NAME "zlib-1.3.2"
#else
#  include "zbuild.h"
#  ifdef ZLIB_COMPAT
#    include "zlib.h"
#  else
#    include "zlib-ng.h"
#  endif
#  define RT_IMPL_NAME "zlib-ng-mmi"
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ps2/bench_display.h"
#include "ps2/roundtrip_metrics.h"
#include "ps2/stress_diagnostic.h"

#if !defined(MIPS_MMI) && !defined(PS2_STOCK_ZLIB)
#  error "Build zlib-ng benchmark with WITH_MMI=ON"
#endif
#define MAX_BENCH (256u * 1024u)
static uint8_t input[MAX_BENCH];
static uint8_t unpacked[MAX_BENCH];
static volatile uint32_t sink;

static void generate(unsigned pattern, size_t len) {
    uint32_t seed = 0x12345678u;
    for (size_t i = 0; i < len; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        switch (pattern) {
        case 0: input[i] = (uint8_t)seed; break;
        case 1: input[i] = (uint8_t)(i & 15u); break;
        case 2: input[i] = 0; break;
        default:
            input[i] = ((i / 4096u) & 1u) ?
                (uint8_t)(i % 31u) : (uint8_t)(seed & 255u);
            break;
        }
    }
}

/* Same original-byte numerator for both phases; these are NOT A/B
 * variants. Every case uses the same verified compressed stream for
 * all six decode samples; samples alternate phase order. No allocations,
 * generated test data, validation or GS writes occur inside timing. */
int main(void) {
    static const char *const names[] = {"compress", "decode"};
    static const size_t lengths[] = {4096U, 65536U, MAX_BENCH};
    static const unsigned iterations[] = {250U, 40U, 12U};
    static const int levels[] = {1, 6, 9};
    z_uintmax_t capacity = PREFIX(compressBound)(MAX_BENCH);
    uint8_t *packed = malloc((size_t)capacity);
    int outcome = 0;
    unsigned valid_cases=0, invalid_cases=0;
    ps2_bench_candidates("roundtrip", names, 2, 0);
    if (!packed) {
        ps2_bench_check(0, 0);
        mmi_stress_fail("roundtrip allocation FAIL capacity=%lu\n",
                        (unsigned long)capacity);
        return 1;
    }
    printf("RT_META,%s,%lu,%u,uncompressed_MB/s,clock()\n",
           RT_IMPL_NAME,(unsigned long)CLOCKS_PER_SEC,PS2_ROUNDTRIP_SAMPLES);
    puts("RT_HEADER,implementation,pattern,level,input_bytes,compressed_bytes,"
         "repetitions,compress_ticks_median,decode_ticks_median,"
         "compress_MBps,decode_MBps");
    for (unsigned pattern = 0; pattern < 4 && !outcome; ++pattern) {
        for (unsigned li = 0; li < 3 && !outcome; ++li) {
            size_t len = lengths[li];
            generate(pattern, len);
            for (unsigned leveli = 0; leveli < 3; ++leveli) {
                unsigned sample, phase;
                int level = levels[leveli], status;
                unsigned count = ps2_bench_iterations(iterations[li]);
                z_uintmax_t used = capacity, decoded = sizeof(unpacked);
                clock_t samples[2][PS2_ROUNDTRIP_SAMPLES];
                double med_c, med_d, rate_c, rate_d;
                ps2_bench_case("roundtrip cases",
                               (pattern * 3U + li) * 3U + leveli + 1U,36U);
                status = PREFIX(compress2)(packed, &used, input,
                                           (z_uintmax_t)len, level);
                ps2_bench_check(0, status == Z_OK);
                if (status != Z_OK) {
                    mmi_stress_fail("RT_FAIL,compress_reference,%u,%d,%lu,%d\n",
                                    pattern,level,(unsigned long)len,status);
                    outcome = 1; break;
                }
                status = PREFIX(uncompress)(unpacked,&decoded,packed,used);
                ps2_bench_check(1,status == Z_OK && decoded == len &&
                                memcmp(input,unpacked,len) == 0);
                if (status != Z_OK || decoded != len ||
                    memcmp(input,unpacked,len) != 0) {
                    mmi_stress_fail("RT_FAIL,decode_reference,%u,%d,%lu,%d\n",
                                    pattern,level,(unsigned long)len,status);
                    outcome = 1; break;
                }
                for (sample = 0; sample < PS2_ROUNDTRIP_SAMPLES; ++sample) {
                    for (unsigned step = 0; step < 2; ++step) {
                        clock_t t0,t1;
                        phase=(sample+step)&1U;
                        t0=clock();
                        if (phase == 0) {
                            for(unsigned i=0; i<count; ++i) {
                                z_uintmax_t size=capacity;
                                status=PREFIX(compress2)(packed,&size,input,
                                                         (z_uintmax_t)len,level);
                                if (status!=Z_OK || size!=used) {
                                    outcome=1; break;
                                }
                                sink ^= (uint32_t)size;
                            }
                        } else {
                            for(unsigned i=0; i<count; ++i) {
                                z_uintmax_t size=sizeof(unpacked);
                                status=PREFIX(uncompress)(unpacked,&size,
                                                          packed,used);
                                if(status!=Z_OK || (size_t)size!=len) {
                                    outcome=1; break;
                                }
                                sink ^= unpacked[i%len];
                            }
                        }
                        t1=clock();
                        samples[phase][sample]=
                            (t0==(clock_t)-1 || t1==(clock_t)-1 || t1<t0)
                                ? (clock_t)-1 : t1-t0;
                        if (outcome) break;
                    }
                    if (outcome) break;
                    /* Six independent correctness gates, outside timers. */
                    if (memcmp(input,unpacked,len)!=0) {
                        outcome=1; break;
                    }
                }
                if (outcome) {
                    ps2_bench_check(0,0);
                    ps2_bench_check(1,0);
                    mmi_stress_fail("RT_FAIL,sample,%u,%d,%lu,%u\n",
                                    pattern,level,(unsigned long)len,sample);
                    break;
                }
                med_c=ps2_roundtrip_median6(samples[0]);
                med_d=ps2_roundtrip_median6(samples[1]);
                if (med_c<=0.0 || med_d<=0.0) {
                    ps2_bench_ticks(0,(clock_t)-1);
                    ps2_bench_ticks(1,(clock_t)-1);
                    ++invalid_cases;
                    printf("RT_INVALID,%u,%d,%lu,clock_unavailable_or_coarse\n",
                           pattern,level,(unsigned long)len);
                    /* Timer invalid is not proof of a performance gain.
                     * Correctness is still independently verified. */
                    continue;
                }
                ++valid_cases;
                rate_c=ps2_roundtrip_mb_s(len,count,med_c);
                rate_d=ps2_roundtrip_mb_s(len,count,med_d);
                ps2_bench_ticks(0,(clock_t)med_c);
                ps2_bench_ticks(1,(clock_t)med_d);
                printf("RT_CASE,%s,%u,%d,%lu,%lu,%u,"
                       "%.3f,%.3f,%.6f,%.6f\n",
                       RT_IMPL_NAME,pattern,level,(unsigned long)len,
                       (unsigned long)used,count,med_c,med_d,rate_c,rate_d);
                ps2_bench_roundtrip_rate(pattern,(unsigned)level,len,
                                         rate_c,rate_d);
            }
        }
    }
    printf("RT_RESULT,%s,valid=%u,invalid=%u,expected=36,checksum=%lu\n",
           outcome?"FAIL":invalid_cases?"PARTIAL":"PASS",
           valid_cases,invalid_cases,(unsigned long)sink);
    free(packed);
    return outcome;
}
