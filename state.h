#ifndef STATE_H
#define STATE_H
#include "types.h"
#include <stdbool.h>
#include <time.h>

/* Execução global */
extern volatile bool g_run;

/* Self */
void       state_init(void);
HouseState state_get_self(void);
void       state_set_self(HouseState s);

/* Cache & presença */
void   cache_set(const HouseState *h);
HouseState cache_get_id(int id);
void   presence_touch(int id);
int    presence_is_online_id(int id); /* 0/1 */
time_t presence_get_last_seen(int id);

/* Grid vars & epoch */
void grid_set_epoch(int epoch);
int  grid_get_epoch(void);
void recompute_grid_vars(GridVars *out);

/* Shedding cmd visto na rede */
void scmd_set(int epoch, int targetOff);
void scmd_get(int *epoch, int *targetOff, time_t *seen_at);

/* Utils */
int clampi(int v, int lo, int hi);
int now_ts(void);
int fairness_score(int id, int epoch);

#endif
