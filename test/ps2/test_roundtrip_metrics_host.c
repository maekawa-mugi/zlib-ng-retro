/* Numeric test only: host can verify all reporting math without PS2SDK. */
#include "roundtrip_metrics.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    clock_t samples[PS2_ROUNDTRIP_SAMPLES]={6,1,5,2,4,3};
    double ticks=ps2_roundtrip_median6(samples);
    double mb=ps2_roundtrip_mb_s(1000000U,2U,
                                (double)CLOCKS_PER_SEC);
    assert(ticks==3.5);
    assert(mb>1.999999 && mb<2.000001);
    samples[0]=(clock_t)-1;
    assert(ps2_roundtrip_median6(samples)<0.0);
    samples[0]=0;
    assert(ps2_roundtrip_median6(samples)<0.0);
    assert(ps2_roundtrip_mb_s(4096U,1U,0.0)==0.0);
    assert(ps2_roundtrip_mb_s(4096U,0U,10.0)==0.0);
    puts("PASS: six-sample median and original-byte MB/s units");
    return 0;
}
