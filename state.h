#ifndef STATE_H
#define STATE_H

#include "types.h"
#include <stdbool.h>
#include <time.h>

extern volatile bool g_run;

void        state_init(void);
DeviceState state_get(void);
void        state_set_setpoint(int duty);
void        state_update_sample(const SensorSample *s);
void        state_inc_i2c_error(void);

/* Utils */
int clampi(int v, int lo, int hi);
int now_ts(void);

#endif
