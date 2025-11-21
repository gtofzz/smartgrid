#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <pthread.h>

#include "config.h"
#include "types.h"
#include "state.h"
#include "cmdq.h"
#include "mqtt_if.h"
#include "control.h"
#include "ui.h"
#include "hw_io.h"

static void on_sigint(int sn) { (void)sn; g_run = false; cmdq_stop(); }

int main(void) {
    signal(SIGINT, on_sigint);

    state_init();
    cmdq_init();
    if (hw_init() != 0) {
        printf("Falha ao iniciar interface I2C\n");
    }

    if (mqtt_start_threads() != 0) {
        printf("Falha ao iniciar MQTT threads\n");
        return 1;
    }

    control_start_threads();

    /* Loop simples de console para telemetria local */
    ui_menu_loop();

    g_run = false;
    cmdq_stop();
    control_join_threads();
    mqtt_join_threads();
    hw_shutdown();

    printf("Encerrado.\n");
    return 0;
}
