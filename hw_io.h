#ifndef HW_IO_H
#define HW_IO_H

#include "types.h"

int hw_init(void);
int hw_send_pwm(int duty);
int hw_read_sample(SensorSample *out);
void hw_shutdown(void);

#endif
