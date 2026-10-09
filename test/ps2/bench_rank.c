#include "bench_rank.h"
#include <stdio.h>
#include <string.h>

void mmi_rank_reset(mmi_rank_state *s) { memset(s, 0, sizeof(*s)); s->active = -1; }
int mmi_rank_start(mmi_rank_state *s, const char *name,
                   const char *const *names, unsigned count, int competitive) {
    if (s->count >= MMI_RANK_GROUPS || count == 0 || count > MMI_RANK_VARIANTS)
        return -1;
    unsigned index = s->count++;
    mmi_rank_group *g = &s->groups[index];
    g->name = name; g->count = count; g->competitive = competitive;
    for (unsigned i = 0; i < count; ++i) g->v[i].name = names[i];
    s->active = (int)index;
    return (int)index;
}
void mmi_rank_check(mmi_rank_state *s, unsigned i, int ok) {
    if (s->active < 0) return;
    mmi_rank_group *g = &s->groups[s->active];
    if (i >= g->count) return;
    ++g->v[i].checks;
    if (!ok) ++g->v[i].failures;
}
void mmi_rank_ticks(mmi_rank_state *s, unsigned i, clock_t ticks) {
    if (s->active < 0) return;
    mmi_rank_group *g = &s->groups[s->active];
    if (i >= g->count) return;
    mmi_rank_variant *v = &g->v[i];
    ++v->samples;
    if (ticks <= 0 || ticks == (clock_t)-1) ++v->invalid;
    else v->ticks += (double)ticks;
}
int mmi_rank_winner(const mmi_rank_group *g) {
    if (!g->competitive || g->count < 2) return -1;
    /* Every contender must have been checked and timed on the identical
     * number of cases. Never call an untested/invalid contender "fastest". */
    unsigned n = g->v[0].samples;
    if (n == 0) return -1;
    int best = -1;
    for (unsigned i = 0; i < g->count; ++i) {
        const mmi_rank_variant *v = &g->v[i];
        if (!v->checks || v->failures || v->invalid || v->samples != n)
            return -1;
        if (best < 0 || v->ticks < g->v[best].ticks) best = (int)i;
    }
    return best;
}
/* Only this precise case is compared. Aggregate weights are intentionally
 * ignored: a 64-byte copy must not get drowned out by a 1KiB copy. */
int mmi_rank_case_winner(const mmi_rank_group *g,
                         const clock_t ticks[MMI_RANK_VARIANTS])
{
    int best=-1;
    unsigned i;
    if (!g->competitive || g->count<2) return -1;
    for(i=0;i<g->count;++i) {
        const mmi_rank_variant *v=&g->v[i];
        if(!v->checks || v->failures || ticks[i]<=0 ||
           ticks[i]==(clock_t)-1) return -1;
        if(best<0 || ticks[i]<ticks[best]) best=(int)i;
    }
    return best;
}
void mmi_rank_report(const mmi_rank_group *g) {
    const int best = mmi_rank_winner(g);
    for (unsigned i = 0; i < g->count; ++i) {
        const mmi_rank_variant *v = &g->v[i];
        printf("MMI_CANDIDATE,%s,%s,%u,%u,%u,%u,%.0f\n", g->name, v->name,
               v->checks, v->failures, v->samples, v->invalid, v->ticks);
    }
    if (best < 0) {
        printf("MMI_WINNER,%s,UNDETERMINED\n", g->name);
    } else {
        double ratio = g->v[best].ticks > 0 ?
            g->v[0].ticks / g->v[best].ticks : 0;
        printf("MMI_WINNER,%s,%s,%.3f\n", g->name, g->v[best].name, ratio);
    }
}
