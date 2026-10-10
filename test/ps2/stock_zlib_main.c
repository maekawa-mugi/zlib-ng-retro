/* Independent PS2 ELF for the unmodified zlib 1.3.2 codec.
 * Compiles the SAME test/bench_mmi_roundtrip.c source as zlib-ng MMI.
 * Do not link both libraries into this ELF: symbols would collide. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "bench_display.h"
#include "third_party/zlib-1.3.2/zlib.h"

extern int ps2_stock_roundtrip_main(void);

int main(int argc, char **argv)
{
    int full=0, fail;
    for (int i=1; i<argc; ++i) {
        if (strcmp(argv[i],"--full")==0) full=1;
        else if (strcmp(argv[i],"--quick")==0) full=0;
        else {
            printf("Usage: %s [--quick|--full]\n",argv[0]);
            return 2;
        }
    }
    ps2_bench_full(full);
    ps2_bench_screen_init();
    ps2_bench_progress("zlib-1.3.2 roundtrip");
    printf("RT_BUILD,original-zlib,1.3.2,PS2_EE,%s\n",
           full?"full":"quick");
    printf("RT_VERSION,%s\n",zlibVersion());
    fail=ps2_stock_roundtrip_main();
    ps2_bench_finish(fail);
    printf("STOCK_ZLIB_RESULT,%s,failures=%d\n",fail?"FAIL":"PASS",fail);
    fflush(stdout);
    ps2_bench_screen_done(fail);
    return fail?1:0;
}
