#include "cmdq.h"
#include <pthread.h>

#define CMDQ_CAP 64
static Command g_cmdq[CMDQ_CAP];
static int g_cmdq_h=0, g_cmdq_t=0, g_cmdq_sz=0;
static pthread_mutex_t g_cmdq_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cmdq_cv   = PTHREAD_COND_INITIALIZER;
static int g_running = 1;

void cmdq_init(void){ g_running = 1; }

void cmdq_push(CommandType t, int arg) {
    pthread_mutex_lock(&g_cmdq_lock);
    if (g_cmdq_sz == CMDQ_CAP) { g_cmdq_t = (g_cmdq_t + 1) % CMDQ_CAP; g_cmdq_sz--; }
    g_cmdq[g_cmdq_h].type = t; g_cmdq[g_cmdq_h].arg = arg;
    g_cmdq_h = (g_cmdq_h + 1) % CMDQ_CAP; g_cmdq_sz++;
    pthread_cond_signal(&g_cmdq_cv);
    pthread_mutex_unlock(&g_cmdq_lock);
}

bool cmdq_pop(Command *out) {
    pthread_mutex_lock(&g_cmdq_lock);
    while (g_cmdq_sz == 0 && g_running) pthread_cond_wait(&g_cmdq_cv, &g_cmdq_lock);
    if (!g_running && g_cmdq_sz == 0) { pthread_mutex_unlock(&g_cmdq_lock); return false; }
    *out = g_cmdq[g_cmdq_t]; g_cmdq_t = (g_cmdq_t + 1) % CMDQ_CAP; g_cmdq_sz--;
    pthread_mutex_unlock(&g_cmdq_lock);
    return true;
}

void cmdq_stop(void){
    pthread_mutex_lock(&g_cmdq_lock);
    g_running = 0;
    pthread_cond_broadcast(&g_cmdq_cv);
    pthread_mutex_unlock(&g_cmdq_lock);
}
