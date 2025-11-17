#include "control.h"
#include "config.h"
#include "types.h"
#include "state.h"
#include "cmdq.h"
#include "logbuf.h"
#include "mqtt_if.h"
#include "hw_io.h"

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

static int last_epoch_seen = -1;
static int shed_applied_this_epoch = 0;

static int cmp_pair(const void *a, const void *b) {
    const int *pa = (const int*)a; const int *pb = (const int*)b;
    return pa[1] - pb[1];
}

/* ---------- PRÉVIA / SELEÇÃO ---------- */
int control_preview_shedding_ids(int epoch, int gap, int *out_ids, int max_out) {
    if (gap <= 0 || max_out <= 0) return 0;
    int targetOff = 2 * gap;

    int pairs[MAX_HOUSES][2]; int n=0;
    for (int id=1; id<MAX_HOUSES; ++id) {
        if (!presence_is_online_id(id)) continue;
        HouseState tmp = cache_get_id(id);
        if (tmp.ID == 0) continue;
        pairs[n][0] = id;
        pairs[n][1] = fairness_score(id, epoch);
        n++;
    }
    qsort(pairs, n, sizeof(pairs[0]), cmp_pair);

    int selected = 0;
    for (int i=0; i<n && selected < targetOff && selected < max_out; ++i) {
        int id = pairs[i][0];
        HouseState h = cache_get_id(id);
        if ((h.c1 + h.c2) == 2) out_ids[selected++] = id;
    }
    return selected;
}

static void compute_and_maybe_act(void) {
    GridVars gv; recompute_grid_vars(&gv);

    /* LED de rede (heurística simples) */
    if (gv.Gap > 2) hw_set_network_led(NETLED_RED);
    else if (gv.Gap > 0 && gv.Gap <=2) hw_set_network_led(NETLED_YELL);
    else hw_set_network_led(NETLED_GREEN);

    int epoch = grid_get_epoch();
    if (epoch != last_epoch_seen) { shed_applied_this_epoch = 0; last_epoch_seen = epoch; }

    int known_epoch=0, known_target=0; time_t seen=0; scmd_get(&known_epoch, &known_target, &seen);

    /* Publica "trace" do shedding no início do epoch com déficit */
    if (gv.Gap > 0 && epoch >= 0 && known_epoch != epoch) {
        int targetOff = 2 * gv.Gap;
        mqtt_publish_shedding_cmd(epoch, targetOff);
        scmd_set(epoch, targetOff);
    }

    /* --- decisão local --- */
    if (gv.Gap > 0) {
        int prev[64]; int nprev = control_preview_shedding_ids(gv.epoch, gv.Gap, prev, 64);
        int my_id = state_get_self().ID;

        int i_should_shed = 0;
        for (int i=0;i<nprev;i++) if (prev[i]==my_id) { i_should_shed = 1; break; }

        if (i_should_shed && !shed_applied_this_epoch) {
            cmdq_push(CMD_INTERNAL_SHED, 0);
            shed_applied_this_epoch = 1;
        }
    } else {
        if (shed_applied_this_epoch) {
            cmdq_push(CMD_INTERNAL_RESTORE, 0);
            shed_applied_this_epoch = 0;
        }
    }
}

/* ---- worker que aplica comandos e sincroniza LEDs ---- */
static void apply_toggle(int *field, const char *name) {
    *field = (*field) ? 0 : 1;
    log_push("[CMD] alternado %s -> %d", name, *field);
}
static void turn_off_one_load(HouseState *s) {
    if (s->c2) s->c2 = 0; else if (s->c1) s->c1 = 0;
}
static void turn_on_one_load(HouseState *s) {
    if (!s->c1) s->c1 = 1; else if (!s->c2) s->c2 = 1;
}

static void *worker_thread(void *arg) {
    (void)arg;
    Command cmd;
    while (g_run && cmdq_pop(&cmd)) {
        HouseState self = state_get_self();
        switch (cmd.type) {
            case CMD_TOGGLE_C1: apply_toggle(&self.c1, "C1"); self.shed=0; self.noOffer=0; break;
            case CMD_TOGGLE_C2: apply_toggle(&self.c2, "C2"); self.shed=0; self.noOffer=0; break;
            case CMD_TOGGLE_SOLAR: apply_toggle(&self.solar, "SOLAR"); break;
            case CMD_SET_ID: {
                int newID = clampi(cmd.arg, 1, MAX_HOUSES-1);
                self.ID = newID; self.shed=0; self.noOffer=0;
            } break;
            case CMD_INTERNAL_SHED: {
                if ((self.c1 + self.c2) >= 1) {
                    int before = self.c1 + self.c2;
                    turn_off_one_load(&self);
                    int after = self.c1 + self.c2;
                    self.shed = (before - after);
                    self.noOffer = (self.shed==0)?1:0;
                } else { self.shed = 0; self.noOffer = 1; }
            } break;
            case CMD_INTERNAL_RESTORE: {
                if ((self.c1 + self.c2) < 2) {
                    turn_on_one_load(&self);
                    self.shed = 0; self.noOffer = 0;
                }
            } break;
            case CMD_QUIT: g_run = false; break;
            default: break;
        }
        self.active=(self.c1||self.c2);
        self.ts = now_ts();
        state_set_self(self);
        cache_set(&self);
        presence_touch(self.ID);

        /* LEDs e publish imediato para refletir já no broker */
        hw_sync_outputs_from_state(self);
        mqtt_publish_state_now();
    }
    return NULL;
}

static void *controller_thread(void *arg) {
    (void)arg;
    while (g_run) {
        compute_and_maybe_act();
        sleep(CTRL_PERIOD_SEC);
    }
    return NULL;
}

void control_start_threads(void){
    pthread_t th_worker, th_ctrl;
    pthread_create(&th_worker, NULL, worker_thread, NULL);
    pthread_detach(th_worker);
    pthread_create(&th_ctrl, NULL, controller_thread, NULL);
    pthread_detach(th_ctrl);
}
void control_join_threads(void){ /* detach */ }
