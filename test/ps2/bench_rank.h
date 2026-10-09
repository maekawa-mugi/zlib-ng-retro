#ifndef PS2_BENCH_RANK_H
#define PS2_BENCH_RANK_H
#include <time.h>
#define MMI_RANK_GROUPS 12
#define MMI_RANK_VARIANTS 12

typedef struct {
    const char *name;
    unsigned checks, failures, samples, invalid;
    double ticks;
} mmi_rank_variant;
typedef struct {
    const char *name;
    unsigned count;
    unsigned competitive; /* False for compression vs decompression phases. */
    mmi_rank_variant v[MMI_RANK_VARIANTS];
} mmi_rank_group;

typedef struct {
    mmi_rank_group groups[MMI_RANK_GROUPS];
    unsigned count;
    int active;
} mmi_rank_state;

void mmi_rank_reset(mmi_rank_state *s);
int mmi_rank_start(mmi_rank_state *s, const char *group,
                   const char *const *names, unsigned count, int competitive);
void mmi_rank_check(mmi_rank_state *s, unsigned variant, int ok);
void mmi_rank_ticks(mmi_rank_state *s, unsigned variant, clock_t ticks);
/* Return -1 if incomplete, mismatched, invalid clock, or incomparable. */
int mmi_rank_winner(const mmi_rank_group *g);
void mmi_rank_report(const mmi_rank_group *g);
#endif
