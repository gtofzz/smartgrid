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
    hw_init();

    if (mqtt_start_threads()!=0){
        printf("Falha ao iniciar MQTT threads\n");
        return 1;
    }

    control_start_threads();
    hw_start_button_thread();

    ui_menu_loop();

    g_run = false;
    cmdq_stop();
    control_join_threads();
    mqtt_join_threads();

    printf("Encerrado.\n");
    return 0;
}
