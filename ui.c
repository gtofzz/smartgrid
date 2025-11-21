#include "ui.h"
#include "cmdq.h"
#include "logbuf.h"
#include "state.h"

#include <stdio.h>
#include <unistd.h>

void ui_menu_loop(void) {
    while (g_run) {
        DeviceState st = state_get();
        printf("\rPWM setpoint: %3d%% | PWM fb: %3d%% | T: %6.2f C | H: %6.2f %% | I2C errs: %d   ",
               st.duty_setpoint, st.last_sample.pwm_feedback,
               st.last_sample.temp_cC/100.0, st.last_sample.hum_cP/100.0,
               st.i2c_errors);
        fflush(stdout);
        sleep(1);
    }
    printf("\nSaindo...\n");
}
