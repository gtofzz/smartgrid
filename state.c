#include "state.h"
#include "config.h"
#include <pthread.h>
#include <string.h>
#include <time.h>

volatile bool g_run = true;

static DeviceState g_state = {
    .duty_setpoint = 0,
    .last_sample = { .temp_cC = 0, .hum_cP = 0, .pwm_feedback = 0 },
    .last_sample_ts = 0,
    .i2c_errors = 0,
};

static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
int now_ts(void) { return (int)time(NULL); }

void state_init(void) {
    pthread_mutex_lock(&g_state_lock);
    g_state.duty_setpoint = 0;
    g_state.last_sample_ts = 0;
    g_state.i2c_errors = 0;
    memset(&g_state.last_sample, 0, sizeof(g_state.last_sample));
    pthread_mutex_unlock(&g_state_lock);
}

DeviceState state_get(void) {
    DeviceState copy;
    pthread_mutex_lock(&g_state_lock);
    copy = g_state;
    pthread_mutex_unlock(&g_state_lock);
    return copy;
}

void state_set_setpoint(int duty) {
    pthread_mutex_lock(&g_state_lock);
    g_state.duty_setpoint = clampi(duty, PWM_MIN, PWM_MAX);
    pthread_mutex_unlock(&g_state_lock);
}

void state_update_sample(const SensorSample *s) {
    if (!s) return;
    pthread_mutex_lock(&g_state_lock);
    g_state.last_sample = *s;
    g_state.last_sample_ts = now_ts();
    pthread_mutex_unlock(&g_state_lock);
}

void state_inc_i2c_error(void) {
    pthread_mutex_lock(&g_state_lock);
    g_state.i2c_errors++;
    pthread_mutex_unlock(&g_state_lock);
}
