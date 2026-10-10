/* Host-only integration adapter: exercise the SAME PS2 roundtrip body
 * against the vendored original zlib, without PS2SDK's GS library. */
#include "bench_display.h"
#include <stdio.h>
#include <time.h>
static unsigned failed_checks;
static unsigned cases;
void ps2_bench_candidates(const char *name, const char *const *names,
                           unsigned count, int competitive)
{
    (void)name; (void)names; (void)count; (void)competitive;
}
void ps2_bench_pair(const char *name, const char *a, const char *b)
{ (void)name; (void)a; (void)b; }
void ps2_bench_check(unsigned variant, int ok)
{ (void)variant; if(!ok) ++failed_checks; }
void ps2_bench_ticks(unsigned variant, clock_t ticks)
{ (void)variant; (void)ticks; }
void ps2_bench_case(const char *name, unsigned current, unsigned total)
{
    (void)name;
    if(total!=36U || current!=++cases) ++failed_checks;
}
unsigned ps2_bench_iterations(unsigned requested)
{
    unsigned n=requested/20U;
    return n?n:1U;
}
void ps2_bench_roundtrip_rate(unsigned pattern,unsigned level,size_t bytes,
                              double compress_mb_s,double decode_mb_s)
{
    (void)pattern; (void)level; (void)bytes;
    if(compress_mb_s<=0.0 || decode_mb_s<=0.0) ++failed_checks;
}
extern int stock_roundtrip_host_main(void);
int main(void)
{
    int rc=stock_roundtrip_host_main();
    if(cases!=36U || failed_checks!=0U || rc!=0) {
        fprintf(stderr,"STOCK_HOST_FAIL,cases=%u,checks=%u,rc=%d\n",
                cases,failed_checks,rc);
        return 1;
    }
    puts("STOCK_HOST_PASS,36/36,original-zlib-1.3.2");
    return 0;
}
