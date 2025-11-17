#ifndef HW_IO_H
#define HW_IO_H
#include "types.h"

/* Inicializa GPIO via sua lib gpio.h */
int  hw_init(void);

/* Aplica LEDs conforme estado local */
void hw_sync_outputs_from_state(HouseState s);

/* Ajusta LED de rede (0=verde,1=amarelo,2=vermelho) */
void hw_set_network_led(int state);

/* Thread que varre botões e gera comandos CMD_TOGGLE_* */
void hw_start_button_thread(void);

#endif
