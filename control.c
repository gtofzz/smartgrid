#include "control.h"
#include "cmdq.h"
#include "config.h"
#include "hw_io.h"
#include "logbuf.h"
#include "state.h"

#include <pthread.h>
#include <unistd.h>

static void *worker_thread(void *arg) {
    (void)arg;
    Command cmd;
    while (g_run && cmdq_pop(&cmd)) {
        if (cmd.type == CMD_QUIT) { g_run = false; break; }

        switch (cmd.type) {
            case CMD_SET_PWM: {
                int duty = clampi(cmd.arg, PWM_MIN, PWM_MAX);
                if (hw_send_pwm(duty) == 0) {
                    state_set_setpoint(duty);
                    log_push("[CTRL] PWM atualizado para %d%%", duty);
                } else {
                    state_inc_i2c_error();
                    log_push("[CTRL] falha ao enviar PWM=%d%%", duty);
                }
            } break;
            case CMD_READ_SENSORS: {
                SensorSample sample;
                if (hw_read_sample(&sample) == 0) {
                    state_update_sample(&sample);
                    log_push("[CTRL] sensores: T=%0.2fC H=%0.2f%% PWMfb=%d%%",
                             sample.temp_cC/100.0, sample.hum_cP/100.0, sample.pwm_feedback);
                } else {
                    state_inc_i2c_error();
                    log_push("[CTRL] falha na leitura I2C dos sensores");
                }
            } break;
            default: break;
        }
    }
    return NULL;
}

static void *poll_thread(void *arg) {
    (void)arg;
    while (g_run) {
        cmdq_push(CMD_READ_SENSORS, 0);
        sleep(SENSOR_PERIOD_SEC);
    }
    return NULL;
}

void control_start_threads(void) {
    pthread_t th_worker, th_poll;
    pthread_create(&th_worker, NULL, worker_thread, NULL);
    pthread_detach(th_worker);
    pthread_create(&th_poll, NULL, poll_thread, NULL);
    pthread_detach(th_poll);
}

void control_join_threads(void) { /* threads detach */ }
