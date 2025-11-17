#include "ui.h"
#include "types.h"
#include "state.h"
#include "config.h"
#include "cmdq.h"
#include "logbuf.h"
#include "control.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void clear_screen(void) { printf("\x1b[2J\x1b[H"); }

static void wait_enter(void) {
    printf("\nPressione ENTER para continuar...");
    fflush(stdout);
    int ch; while ((ch = getchar()) != '\n' && ch != EOF) { }
}

static int cmp_pair(const void *a, const void *b) {
    const int *pa = (const int*)a; const int *pb = (const int*)b;
    return pa[1] - pb[1];
}

static void show_debug_global(void) {
    clear_screen();
    printf("=== DEBUG: Estados das outras casas (online) ===\n\n");
    printf("ID  C1 C2 SOL ACT SH NO lastSeen(age s)\n");
    printf("---------------------------------------\n");

    time_t now = time(NULL);
    for (int id=1; id<MAX_HOUSES; ++id) {
        if (id == state_get_self().ID) continue;
        const HouseState h = cache_get_id(id);
        if (h.ID == 0) continue;
        time_t seen = presence_get_last_seen(id);
        int age = (seen>0) ? (int)(now - seen) : -1;
        if (age < 0 || age > NODE_TTL_SEC) continue; // offline
        printf("%2d  %2d %2d  %2d  %2d  %2d  %2d  %ld(%d)\n",
               h.ID, h.c1, h.c2, h.solar, (h.c1||h.c2), h.shed, h.noOffer, (long)seen, age);
    }
    wait_enter();
}

static void show_debug_shedding(void) {
    clear_screen();
    GridVars gv; recompute_grid_vars(&gv);

    int epoch = grid_get_epoch();
    int known_epoch=0, known_target=0; time_t seen=0; scmd_get(&known_epoch, &known_target, &seen);

    printf("=== DEBUG: Shedding colaborativo ===\n\n");
    printf("epoch=%d  H=%d  G=%d  Loff=%d  Cap=%d  Gap=%d\n", epoch, gv.H, gv.G, gv.Loff, gv.Cap, gv.Gap);
    if (known_epoch == epoch) printf("CMD: targetOff=%d\n", known_target);
    else printf("CMD: nenhum para este epoch\n");

    int pairs[MAX_HOUSES][2]; int n=0;
    for (int id=1; id<MAX_HOUSES; ++id) {
        HouseState tmp = cache_get_id(id);
        if (tmp.ID == 0) continue;
        if (!presence_is_online_id(id)) continue;
        pairs[n][0] = id;
        pairs[n][1] = fairness_score(id, epoch);
        n++;
    }
    qsort(pairs, n, sizeof(pairs[0]), cmp_pair);

    printf("\nOrdem de fairness (id:score):\n");
    for (int i=0; i<n; ++i) {
        int star = (pairs[i][0]==state_get_self().ID);
        printf("%d:%d%s ", pairs[i][0], pairs[i][1], (star?"*":""));
    }
    printf("\n");

    if (known_epoch == epoch && known_target > 0) {
        printf("\nSeleção (até %d que podem oferecer):\n", known_target);
        int selected=0;
        for (int i=0; i<n && selected < known_target; ++i) {
            int id = pairs[i][0];
            HouseState h = cache_get_id(id);
            if ((h.c1 + h.c2) == 2) {
                printf("#%d id=%d (oferece)\n", selected+1, id);
                selected++;
            } else {
                printf("--  id=%d (não oferece: cargas=%d)\n", id, h.c1+h.c2);
            }
        }
    }

    printf("\nEventos recentes:\n");
    log_dump_recent(64);
    wait_enter();
}

void ui_menu_loop(void) {
    while (g_run) {
        clear_screen();
        HouseState self = state_get_self();
        GridVars gv; recompute_grid_vars(&gv);
        int epoch = grid_get_epoch();

        int preview[32];
        int nprev = control_preview_shedding_ids(gv.epoch, gv.Gap, preview, 32);

        printf("===== SMART GRID COLABORATIVO =====\n");
        printf("Casa atual: ID=%d   Epoch(lido)=%d\n", self.ID, epoch);
        printf("Cargas: C1=%d  C2=%d  Solar=%d  Active=%d\n", self.c1, self.c2, self.solar, (self.c1||self.c2));
        printf("GAP=%d  (H=%d  Cap=%d  G=%d)\n", gv.Gap, gv.H, gv.Cap, gv.G);
        if (nprev > 0) {
            printf("Deveriam desligar agora (até 2*Gap): ");
            for (int i=0;i<nprev;i++){ printf("%s%d", (i? ", ":""), preview[i]); }
            printf("\n");
        } else if (gv.Gap > 0) {
            printf("Deveriam desligar: sem candidatos com 2 cargas ON\n");
        } else {
            printf("Deveriam desligar: ninguém (Gap<=0)\n");
        }
        printf("-----------------------------------\n");
        printf("1) Alternar C1\n");
        printf("2) Alternar C2\n");
        printf("3) Alternar Solar\n");
        printf("4) Mudar ID da casa\n");
        printf("5) Debug\n");
        printf("6) Sair\n\n");
        printf("Escolha: "); fflush(stdout);

        char line[32];
        if (!fgets(line, sizeof(line), stdin)) continue;
        int opt = atoi(line);
        if (opt == 1) cmdq_push(CMD_TOGGLE_C1, 0);
        else if (opt == 2) cmdq_push(CMD_TOGGLE_C2, 0);
        else if (opt == 3) cmdq_push(CMD_TOGGLE_SOLAR, 0);
        else if (opt == 4) {
            printf("Novo ID (1..%d): ", MAX_HOUSES-1); fflush(stdout);
            if (fgets(line, sizeof(line), stdin)) { int nid = atoi(line); cmdq_push(CMD_SET_ID, nid); }
        }
        else if (opt == 5) {
            clear_screen();
            printf("=== DEBUG MENU ===\n");
            printf("1) Ver estados das outras casas\n");
            printf("2) Ver shed colaborativo (trace)\n");
            printf("3) Voltar\n");
            printf("Escolha: "); fflush(stdout);
            if (fgets(line, sizeof(line), stdin)) {
                int d = atoi(line);
                if (d == 1) show_debug_global();
                else if (d == 2) show_debug_shedding();
            }
        }
        else if (opt == 6) { cmdq_push(CMD_QUIT, 0); break; }
    }
}
