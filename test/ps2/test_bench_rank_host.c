/* Host-only, no EE or PS2SDK required. */
#include "bench_rank.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    mmi_rank_state s;
    const char *names[] = {"baseline", "fast", "slow"};
    mmi_rank_reset(&s);
    assert(mmi_rank_start(&s,"test", names, 3, 1) == 0);
    assert(mmi_rank_winner(&s.groups[0]) == -1);
    for(unsigned i=0;i<3;i++) {
        mmi_rank_check(&s,i,1);
        mmi_rank_ticks(&s,i,(clock_t)(100-i*25));
    }
    assert(mmi_rank_winner(&s.groups[0]) == 2);
    mmi_rank_ticks(&s,2,4); /* Unequal sample counts must not rank. */
    assert(mmi_rank_winner(&s.groups[0]) == -1);
    mmi_rank_reset(&s);
    assert(mmi_rank_start(&s,"test",names,3,1) == 0);
    for(unsigned i=0;i<3;i++) {
        mmi_rank_check(&s,i,1);
        mmi_rank_ticks(&s,i,(clock_t)(100+i*10));
    }
    assert(mmi_rank_winner(&s.groups[0]) == 0);
    mmi_rank_check(&s,1,0);
    assert(mmi_rank_winner(&s.groups[0]) == -1);
    mmi_rank_reset(&s);
    assert(mmi_rank_start(&s,"phases",names,2,0) == 0);
    for(unsigned i=0;i<2;i++) {mmi_rank_check(&s,i,1); mmi_rank_ticks(&s,i,20);}
    assert(mmi_rank_winner(&s.groups[0]) == -1); /* No compression vs decode ranking. */
    mmi_rank_reset(&s);
    assert(mmi_rank_start(&s,"test",names,2,1) == 0);
    mmi_rank_check(&s,0,1); mmi_rank_check(&s,1,1);
    mmi_rank_ticks(&s,0,100); mmi_rank_ticks(&s,1,0);
    assert(mmi_rank_winner(&s.groups[0]) == -1);
    mmi_rank_report(&s.groups[0]);
    puts("PS2 rank host: PASS");
    return 0;
}
