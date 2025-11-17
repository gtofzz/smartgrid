#include "logbuf.h"
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>

#define LOG_CAP 256
static char g_logs[LOG_CAP][256];
static int  g_log_head = 0;
static int  g_log_len  = 0;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

void log_push(const char *fmt, ...) {
    pthread_mutex_lock(&g_log_lock);
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_logs[g_log_head], sizeof(g_logs[g_log_head]), fmt, ap);
    va_end(ap);
    g_log_head = (g_log_head + 1) % LOG_CAP;
    if (g_log_len < LOG_CAP) g_log_len++;
    pthread_mutex_unlock(&g_log_lock);
}

void log_dump_recent(int max_lines){
    pthread_mutex_lock(&g_log_lock);
    int count = (max_lines <= 0 || max_lines > g_log_len) ? g_log_len : max_lines;
    for (int i=0; i<count; ++i) {
        int idx = (g_log_head - 1 - i + LOG_CAP) % LOG_CAP;
        printf("- %s\n", g_logs[idx]);
    }
    pthread_mutex_unlock(&g_log_lock);
}
