#include "hw_io.h"
#include "config.h"
#include "types.h"
#include "state.h"
#include "cmdq.h"
#include "logbuf.h"

/* SUA biblioteca */
#include "gpio.h"

#include <pthread.h>
#include <unistd.h>

int hw_init(void){
    gpio_setup(); /* sua função */
    HouseState s = state_get_self();
    hw_sync_outputs_from_state(s);
    hw_set_network_led(NETLED_GREEN);
    return 0;
}

void hw_sync_outputs_from_state(HouseState s){
    set_led(LEDC1, s.c1 ? 1 : 0);
    set_led(LEDC2, s.c2 ? 1 : 0);
    set_led(LEDSOL, s.solar ? 1 : 0);
}
void hw_set_network_led(int state){
    set_network_led(state);
}

static void *btn_thread(void *arg){
    (void)arg;
    int pins[3] = { BOTAOC1, BOTAOC2, BOTAOSOL };
    int st [3]  = {0,0,0};
    int cnt[3]  = {0,0,0};

    while (g_run) {
        for (int i=0;i<3;i++){
            int lev = read_button(pins[i]) ? 1 : 0;
            if (lev == st[i]) cnt[i] = 0;
            else {
                cnt[i]++;
                if (cnt[i] >= BTN_STABLE_TICKS) {
                    st[i] = lev; cnt[i]=0;
                    if (st[i] == 1) { /* borda de pressão 0->1 */
                        if (i==0) cmdq_push(CMD_TOGGLE_C1, 0);
                        if (i==1) cmdq_push(CMD_TOGGLE_C2, 0);
                        if (i==2) cmdq_push(CMD_TOGGLE_SOLAR, 0);
                    }
                }
            }
        }
        usleep(BTN_POLL_MS*1000);
    }
    return NULL;
}

void hw_start_button_thread(void){
    pthread_t th; pthread_create(&th, NULL, btn_thread, NULL); pthread_detach(th);
}
