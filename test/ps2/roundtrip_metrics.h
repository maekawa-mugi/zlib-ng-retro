/* Portable numeric helpers for PS2/host whole-stream A/B reports.
 * The "throughput" denominator is original uncompressed bytes for BOTH
 * compression and decompression. Use median elapsed ticks, not the
 * median of rates; this makes pairwise time comparisons unambiguous. */
#ifndef PS2_ROUNDTRIP_METRICS_H
#define PS2_ROUNDTRIP_METRICS_H
#include <stddef.h>
#include <time.h>
#define PS2_ROUNDTRIP_SAMPLES 6U

static double ps2_roundtrip_median6(const clock_t samples[PS2_ROUNDTRIP_SAMPLES])
{
    double sorted[PS2_ROUNDTRIP_SAMPLES], value;
    unsigned i,j;
    for(i=0;i<PS2_ROUNDTRIP_SAMPLES;++i) {
        if(samples[i] <= 0 || samples[i] == (clock_t)-1)
            return -1.0;
        value=(double)samples[i];
        for(j=i;j>0 && sorted[j-1]>value;--j)
            sorted[j]=sorted[j-1];
        sorted[j]=value;
    }
    return (sorted[2]+sorted[3])*0.5;
}

static double ps2_roundtrip_mb_s(size_t bytes, unsigned repetitions,
                                  double median_ticks)
{
    if(median_ticks <= 0.0 || repetitions == 0 || CLOCKS_PER_SEC <= 0)
        return 0.0;
    return ((double)bytes*(double)repetitions*(double)CLOCKS_PER_SEC) /
           (median_ticks*1000000.0);
}
#endif
