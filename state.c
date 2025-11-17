#include "state.h"
#include "config.h"
#include <pthread.h>
#include <string.h>
#include <time.h>

volatile bool g_run = true;

/* Self inicial */
static HouseState g_self = { .ID = 2, .c1 = 1, .c2 = 0, .solar = 0, .active = 1, .shed = 0, .noOffer = 0, .ts = 0 };

/* Cache e locks */
static HouseState g_cache[MAX_HOUSES]; // índice = ID
static pthread_rwlock_t g_cache_lock = PTHREAD_RWLOCK_INITIALIZER;

/* Grid vars & shedding command */
static struct { int epoch; pthread_mutex_t m; } g_grid = { .epoch = 0, .m = PTHREAD_MUTEX_INITIALIZER };
static SheddingCmd g_scmd = { .epoch = -1, .targetOff = 0, .seen_at = 0 };
static pthread_mutex_t g_scmd_lock  = PTHREAD_MUTEX_INITIALIZER;

/* presença por ID */
static time_t g_seen_at[MAX_HOUSES] = {0};

/* lock do self */
static pthread_mutex_t  g_self_lock  = PTHREAD_MUTEX_INITIALIZER;

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
int now_ts(void) { return (int)time(NULL); }
int fairness_score(int id, int epoch) { return ((epoch*13) + (id*7)) % 100; }

void state_init(void){
    memset(g_cache, 0, sizeof(g_cache));
    pthread_mutex_lock(&g_self_lock);
    g_self.active = (g_self.c1 || g_self.c2);
    g_self.ts = now_ts();
    HouseState self_copy = g_self;
    pthread_mutex_unlock(&g_self_lock);
    cache_set(&self_copy);
    presence_touch(self_copy.ID);
}

HouseState state_get_self(void){
    HouseState s;
    pthread_mutex_lock(&g_self_lock);
    s = g_self;
    pthread_mutex_unlock(&g_self_lock);
    return s;
}

void state_set_self(HouseState s){
    pthread_mutex_lock(&g_self_lock);
    g_self = s;
    pthread_mutex_unlock(&g_self_lock);
}

void cache_set(const HouseState *h) {
    if (!h || h->ID <= 0 || h->ID >= MAX_HOUSES) return;
    pthread_rwlock_wrlock(&g_cache_lock);
    g_cache[h->ID] = *h;
    pthread_rwlock_unlock(&g_cache_lock);
}

HouseState cache_get_id(int id) {
    HouseState out = (HouseState){0};
    if (id <= 0 || id >= MAX_HOUSES) return out;
    pthread_rwlock_rdlock(&g_cache_lock);
    out = g_cache[id];
    pthread_rwlock_unlock(&g_cache_lock);
    return out;
}

void presence_touch(int id){
    if (id > 0 && id < MAX_HOUSES) g_seen_at[id] = time(NULL);
}
int presence_is_online_id(int id){
    if (id <= 0 || id >= MAX_HOUSES) return 0;
    time_t t = g_seen_at[id];
    return (t!=0) && ((time(NULL)-t) <= NODE_TTL_SEC);
}
time_t presence_get_last_seen(int id){
    if (id <= 0 || id >= MAX_HOUSES) return 0;
    return g_seen_at[id];
}

void grid_set_epoch(int epoch){
    pthread_mutex_lock(&g_grid.m);
    g_grid.epoch = epoch;
    pthread_mutex_unlock(&g_grid.m);
}
int grid_get_epoch(void){
    int e;
    pthread_mutex_lock(&g_grid.m);
    e = g_grid.epoch;
    pthread_mutex_unlock(&g_grid.m);
    return e;
}

void scmd_set(int epoch, int targetOff){
    pthread_mutex_lock(&g_scmd_lock);
    g_scmd.epoch = epoch;
    g_scmd.targetOff = targetOff;
    g_scmd.seen_at = time(NULL);
    pthread_mutex_unlock(&g_scmd_lock);
}
void scmd_get(int *epoch, int *targetOff, time_t *seen_at){
    pthread_mutex_lock(&g_scmd_lock);
    if (epoch) *epoch = g_scmd.epoch;
    if (targetOff) *targetOff = g_scmd.targetOff;
    if (seen_at) *seen_at = g_scmd.seen_at;
    pthread_mutex_unlock(&g_scmd_lock);
}

void recompute_grid_vars(GridVars *out) {
    int H=0, G=0, Loff=0, epoch = grid_get_epoch();

    pthread_rwlock_rdlock(&g_cache_lock);
    for (int id=1; id<MAX_HOUSES; ++id) {
        const HouseState *h = &g_cache[id];
        if (h->ID == 0) continue;
        if (!presence_is_online_id(id)) continue;
        H += ((h->c1||h->c2) ? 1:0);
        G += (h->solar ? 1:0);
        Loff += h->shed;
    }
    pthread_rwlock_unlock(&g_cache_lock);

    int Cap = BASE_CAP + G + (Loff/4);
    int Gap = H - Cap;

    out->H = H; out->G = G; out->Loff = Loff; out->Cap = Cap; out->Gap = Gap; out->epoch = epoch; out->targetOff = 0;
}
